/**
 * Finvasia / Shoonya (NorenApi) C++ Client
 *
 * Converted from api_helper.py + NorenRestApiPy.NorenApi.
 * Uses the Noren protocol: all requests are POST with
 *   jData=<JSON-encoded params>&jKey=<session token>
 *
 * Dependencies:
 *   - libcurl       (HTTP requests)
 *   - nlohmann/json (JSON library)
 *   - OpenSSL       (SHA-256 for login)
 *
 * Compile:
 *   g++ -std=c++17 -o finvasia finvasia.cpp -lcurl -lssl -lcrypto
 *   (MSVC: cl /std:c++17 /EHsc finvasia.cpp libcurl.lib libssl.lib
 *   libcrypto.lib)
 */

#include <algorithm>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include <curl/curl.h>
#include <openssl/sha.h>

#include <nlohmann/json.hpp>

using json = nlohmann::json;

// ──────────────────────────────────────────────
//  libcurl Write Callback
// ──────────────────────────────────────────────

static size_t WriteCallback(void *contents, size_t size, size_t nmemb,
                            void *userp) {
  static_cast<std::string *>(userp)->append(static_cast<char *>(contents),
                                            size * nmemb);
  return size * nmemb;
}

// ──────────────────────────────────────────────
//  FinvasiaBroker Class (Shoonya / NorenApi)
// ──────────────────────────────────────────────

class FinvasiaBroker {
public:
  /**
   * Constructor
   *
   * @param host  Base URL (default: Shoonya production)
   */
  FinvasiaBroker(
      const std::string &host = "https://api.shoonya.com/NorenWClientTP/")
      : host_(host) {
    // Ensure trailing slash
    if (!host_.empty() && host_.back() != '/')
      host_ += '/';
    std::cout << "Finvasia (Shoonya) Broker initialized." << std::endl;
  }

  // ─── Authentication ─────────────────────────

  /**
   * Login via QuickAuth.
   *
   * @param userId     User ID (e.g. "FA12345")
   * @param password   Plain text password (will be SHA-256'd)
   * @param factor2    2FA / TOTP code
   * @param vendorCode Vendor code (vc)
   * @param apiKey     API key (used to build appkey = SHA256(uid|apikey))
   * @param imei       IMEI or unique device ID
   * @param source     "API" (default)
   * @return JSON with susertoken, etc.
   */
  json login(const std::string &userId, const std::string &password,
             const std::string &factor2, const std::string &vendorCode,
             const std::string &apiKey, const std::string &imei = "abc1234",
             const std::string &source = "API") {
    userId_ = userId;

    // password → SHA-256
    std::string pwdHash = sha256(password);

    // appkey → SHA-256(userId|apiKey)
    std::string appkey = sha256(userId + "|" + apiKey);

    json jData = {{"apkversion", "1.0.0"}, {"uid", userId},
                  {"pwd", pwdHash},        {"factor2", factor2},
                  {"vc", vendorCode},      {"appkey", appkey},
                  {"imei", imei},          {"source", source}};

    json response = norenPost("QuickAuth", jData, false);
    std::cout << "login response: " << response.dump(2) << std::endl;

    if (response.contains("stat") &&
        response["stat"].get<std::string>() == "Ok") {
      sessionToken_ = response.value("susertoken", "");
    }

    return response;
  }

  /** Logout. */
  json logout() {
    json jData = {{"uid", userId_}};
    json response = norenPost("Logout", jData, true);
    std::cout << "logout response: " << response.dump(2) << std::endl;
    sessionToken_ = "";
    return response;
  }

  // ─── User / Account ─────────────────────────

  /** Get user details / profile. */
  json getUserDetails() {
    json jData = {{"uid", userId_}};
    json response = norenPost("UserDetails", jData, true);
    std::cout << "userDetails response: " << response.dump(2) << std::endl;
    return response;
  }

  /** Get fund limits / balance. */
  json getLimits() {
    json jData = {{"uid", userId_}, {"actid", userId_}};
    json response = norenPost("Limits", jData, true);
    std::cout << "limits response: " << response.dump(2) << std::endl;
    return response;
  }

  // ─── Orders ─────────────────────────────────

  /**
   * Place an order.
   *
   * @param buyOrSell      "B" (buy) or "S" (sell)
   * @param productType    "C" (CNC), "M" (NRML), "I" (MIS), "B" (bracket),
   *                       "H" (cover)
   * @param exchange       "NSE", "BSE", "NFO", "CDS", "MCX"
   * @param tradingSymbol  e.g. "RELIANCE-EQ" or "NIFTY23FEB18000CE"
   * @param quantity       Order quantity
   * @param priceType      "MKT", "LMT", "SL-MKT", "SL-LMT"
   * @param price          Limit price (0 for market)
   * @param triggerPrice   Trigger price for SL orders (0 if none)
   * @param discloseQty    Disclosed quantity
   * @param retention      "DAY" or "IOC"
   * @param remarks        Order tag / remarks
   */
  json placeOrder(const std::string &buyOrSell, const std::string &productType,
                  const std::string &exchange, const std::string &tradingSymbol,
                  int quantity, const std::string &priceType, double price = 0,
                  double triggerPrice = 0, int discloseQty = 0,
                  const std::string &retention = "DAY",
                  const std::string &remarks = "tag") {
    json jData = {{"uid", userId_},
                  {"actid", userId_},
                  {"exch", exchange},
                  {"tsym", tradingSymbol},
                  {"qty", std::to_string(quantity)},
                  {"prc", std::to_string(price)},
                  {"prd", productType},
                  {"trantype", buyOrSell},
                  {"prctyp", priceType},
                  {"ret", retention}};

    if (triggerPrice > 0)
      jData["trgprc"] = std::to_string(triggerPrice);
    if (discloseQty > 0)
      jData["dscqty"] = std::to_string(discloseQty);
    if (!remarks.empty())
      jData["remarks"] = remarks;

    json response = norenPost("PlaceOrder", jData, true);
    std::cout << "placeOrder response: " << response.dump(2) << std::endl;
    return response;
  }

  /**
   * Modify an order.
   *
   * @param orderId       Noren order number
   * @param exchange      Exchange
   * @param tradingSymbol Trading symbol
   * @param quantity      New quantity
   * @param priceType     New price type
   * @param price         New price
   * @param triggerPrice  New trigger price
   */
  json modifyOrder(const std::string &orderId, const std::string &exchange,
                   const std::string &tradingSymbol, int quantity,
                   const std::string &priceType, double price = 0,
                   double triggerPrice = 0) {
    json jData = {{"uid", userId_},
                  {"norenordno", orderId},
                  {"exch", exchange},
                  {"tsym", tradingSymbol},
                  {"qty", std::to_string(quantity)},
                  {"prc", std::to_string(price)},
                  {"prctyp", priceType}};

    if (triggerPrice > 0)
      jData["trgprc"] = std::to_string(triggerPrice);

    json response = norenPost("ModifyOrder", jData, true);
    std::cout << "modifyOrder response: " << response.dump(2) << std::endl;
    return response;
  }

  /** Cancel an order. */
  json cancelOrder(const std::string &orderId) {
    json jData = {{"uid", userId_}, {"norenordno", orderId}};
    json response = norenPost("CancelOrder", jData, true);
    std::cout << "cancelOrder response: " << response.dump(2) << std::endl;
    return response;
  }

  /** Exit a bracket / cover order. */
  json exitSnoOrder(const std::string &orderId,
                    const std::string &productType) {
    json jData = {
        {"uid", userId_}, {"norenordno", orderId}, {"prd", productType}};
    json response = norenPost("ExitSNOOrder", jData, true);
    std::cout << "exitSnoOrder response: " << response.dump(2) << std::endl;
    return response;
  }

  // ─── Books ──────────────────────────────────

  /** Get order book. */
  json getOrderBook() {
    json jData = {{"uid", userId_}};
    json response = norenPost("OrderBook", jData, true);
    std::cout << "orderBook response: " << response.dump(2) << std::endl;
    return response;
  }

  /** Get single order history. */
  json getSingleOrderHistory(const std::string &orderId) {
    json jData = {{"uid", userId_}, {"norenordno", orderId}};
    json response = norenPost("SingleOrderHistory", jData, true);
    std::cout << "singleOrderHistory response: " << response.dump(2)
              << std::endl;
    return response;
  }

  /** Get trade book. */
  json getTradeBook() {
    json jData = {{"uid", userId_}};
    json response = norenPost("TradeBook", jData, true);
    std::cout << "tradeBook response: " << response.dump(2) << std::endl;
    return response;
  }

  // ─── Portfolio ──────────────────────────────

  /** Get position book. */
  json getPositionBook() {
    json jData = {{"uid", userId_}, {"actid", userId_}};
    json response = norenPost("PositionBook", jData, true);
    std::cout << "positionBook response: " << response.dump(2) << std::endl;
    return response;
  }

  /** Get holdings. */
  json getHoldings(const std::string &productType = "") {
    json jData = {{"uid", userId_}, {"actid", userId_}};
    if (!productType.empty())
      jData["prd"] = productType;
    json response = norenPost("Holdings", jData, true);
    std::cout << "holdings response: " << response.dump(2) << std::endl;
    return response;
  }

  /**
   * Convert product type of a position.
   *
   * @param exchange       Exchange
   * @param tradingSymbol  Trading symbol
   * @param quantity       Quantity to convert
   * @param fromPrd        Current product type
   * @param toPrd          Target product type
   * @param tranType       "B" or "S" (direction of the position)
   * @param posType        "DAY" or "CF"
   */
  json productConversion(const std::string &exchange,
                         const std::string &tradingSymbol, int quantity,
                         const std::string &fromPrd, const std::string &toPrd,
                         const std::string &tranType,
                         const std::string &posType = "DAY") {
    json jData = {{"uid", userId_},
                  {"actid", userId_},
                  {"exch", exchange},
                  {"tsym", tradingSymbol},
                  {"qty", std::to_string(quantity)},
                  {"prd", fromPrd},
                  {"prevprd", fromPrd},
                  {"newprd", toPrd},
                  {"trantype", tranType},
                  {"postype", posType}};

    json response = norenPost("ProductConversion", jData, true);
    std::cout << "productConversion response: " << response.dump(2)
              << std::endl;
    return response;
  }

  // ─── Market Data ────────────────────────────

  /**
   * Get quotes / LTP for a symbol.
   *
   * @param exchange       Exchange (e.g. "NSE")
   * @param token          Scrip token (e.g. "22")
   */
  json getQuotes(const std::string &exchange, const std::string &token) {
    json jData = {{"uid", userId_}, {"exch", exchange}, {"token", token}};
    json response = norenPost("GetQuotes", jData, true);
    std::cout << "getQuotes response: " << response.dump(2) << std::endl;
    return response;
  }

  /**
   * Search for scrips / instruments.
   *
   * @param exchange    Exchange (e.g. "NSE", "NFO")
   * @param searchText  Search text (e.g. "RELIANCE")
   */
  json searchScrip(const std::string &exchange, const std::string &searchText) {
    json jData = {{"uid", userId_}, {"exch", exchange}, {"stext", searchText}};
    json response = norenPost("SearchScrip", jData, true);
    std::cout << "searchScrip response: " << response.dump(2) << std::endl;
    return response;
  }

  /**
   * Get historical / time price series data.
   *
   * @param exchange     Exchange
   * @param token        Scrip token
   * @param startTime    Epoch start time
   * @param endTime      Epoch end time (0 = now)
   * @param interval     "1", "3", "5", "10", "15", "30", "60", "120", "240"
   */
  json getTimePriceSeries(const std::string &exchange, const std::string &token,
                          const std::string &startTime,
                          const std::string &endTime = "",
                          const std::string &interval = "1") {
    json jData = {{"uid", userId_},
                  {"exch", exchange},
                  {"token", token},
                  {"st", startTime}};

    if (!endTime.empty())
      jData["et"] = endTime;
    if (interval != "1")
      jData["intrv"] = interval;

    json response = norenPost("TPSeries", jData, true);
    std::cout << "timePriceSeries response: " << response.dump(2) << std::endl;
    return response;
  }

  /**
   * Get daily price series (EOD data).
   */
  json getDailyPriceSeries(const std::string &exchange,
                           const std::string &tradingSymbol,
                           const std::string &startDate,
                           const std::string &endDate = "") {
    json jData = {{"uid", userId_},
                  {"sym", exchange + ":" + tradingSymbol},
                  {"from", startDate}};

    if (!endDate.empty())
      jData["to"] = endDate;

    json response = norenPost("EODChartData", jData, true);
    std::cout << "dailyPriceSeries response: " << response.dump(2) << std::endl;
    return response;
  }

  /** Get option chain for a given index/scrip. */
  json getOptionChain(const std::string &exchange,
                      const std::string &tradingSymbol, double strikePrice,
                      int count = 5) {
    json jData = {{"uid", userId_},
                  {"exch", exchange},
                  {"tsym", tradingSymbol},
                  {"strprc", std::to_string(strikePrice)},
                  {"cnt", std::to_string(count)}};
    json response = norenPost("GetOptionChain", jData, true);
    std::cout << "optionChain response: " << response.dump(2) << std::endl;
    return response;
  }

  // ─── Getters / Setters ──────────────────────

  std::string getSessionToken() const { return sessionToken_; }
  std::string getUserId() const { return userId_; }

  void setSessionToken(const std::string &token) { sessionToken_ = token; }
  void setUserId(const std::string &uid) { userId_ = uid; }

private:
  std::string host_;
  std::string userId_;
  std::string sessionToken_;

  // ─── Utility ────────────────────────────────

  /** SHA-256 hash as hex string. */
  static std::string sha256(const std::string &input) {
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256(reinterpret_cast<const unsigned char *>(input.c_str()), input.size(),
           hash);

    std::ostringstream oss;
    for (int i = 0; i < SHA256_DIGEST_LENGTH; ++i)
      oss << std::hex << std::setfill('0') << std::setw(2)
          << static_cast<int>(hash[i]);
    return oss.str();
  }

  /** URL-encode a string. */
  static std::string urlEncode(const std::string &value) {
    CURL *curl = curl_easy_init();
    if (!curl)
      return value;
    char *encoded =
        curl_easy_escape(curl, value.c_str(), static_cast<int>(value.length()));
    std::string result(encoded);
    curl_free(encoded);
    curl_easy_cleanup(curl);
    return result;
  }

  // ─── HTTP Helpers ───────────────────────────

  /**
   * Noren-style POST request.
   * All Noren APIs send form-encoded body:
   *   jData=<URL-encoded JSON>&jKey=<session token>
   *
   * @param endpoint  API endpoint name (e.g. "PlaceOrder")
   * @param jData     JSON object to send as jData
   * @param withKey   Whether to include jKey (session token)
   */
  json norenPost(const std::string &endpoint, const json &jData, bool withKey) {
    std::string url = host_ + endpoint;
    std::string jsonStr = jData.dump();

    // Build form body: jData=<json>
    std::string body = "jData=" + urlEncode(jsonStr);
    if (withKey && !sessionToken_.empty())
      body += "&jKey=" + urlEncode(sessionToken_);

    CURL *curl = curl_easy_init();
    if (!curl)
      throw std::runtime_error("Failed to initialize libcurl");

    std::string responseStr;

    // Noren uses application/x-www-form-urlencoded
    struct curl_slist *headers = nullptr;
    headers = curl_slist_append(
        headers, "Content-Type: application/x-www-form-urlencoded");

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &responseStr);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);

    CURLcode res = curl_easy_perform(curl);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK)
      throw std::runtime_error(std::string("HTTP POST failed: ") +
                               curl_easy_strerror(res));

    return parseResponse(responseStr);
  }

  /** Parse response JSON. */
  json parseResponse(const std::string &responseStr) {
    try {
      return json::parse(responseStr);
    } catch (const json::parse_error &e) {
      std::cerr << "JSON parse error: " << e.what() << std::endl;
      std::cerr << "Raw response: " << responseStr << std::endl;
      throw;
    }
  }
};

// ──────────────────────────────────────────────
//  main() – Usage demo
// ──────────────────────────────────────────────

// int main() {
//   curl_global_init(CURL_GLOBAL_DEFAULT);

//   try {
//     // Default Shoonya production host
//     FinvasiaBroker broker;

//     // ── Step 1: Login ──
//     std::cout << "\n=== Login ===" << std::endl;
//     json loginResp = broker.login(
//         /* userId     */ "YOUR_USER_ID",
//         /* password   */ "YOUR_PASSWORD",
//         /* factor2    */ "YOUR_TOTP",
//         /* vendorCode */ "YOUR_VENDOR_CODE",
//         /* apiKey     */ "YOUR_API_KEY");
//     std::cout << loginResp.dump(2) << std::endl;

//     // ── Example: Get limits / balance ──
//     std::cout << "\n=== Limits ===" << std::endl;
//     json limits = broker.getLimits();
//     std::cout << limits.dump(2) << std::endl;

//     // ── Example: Place a market order ──
//     std::cout << "\n=== Place Order ===" << std::endl;
//     json orderResp = broker.placeOrder(
//         /* buyOrSell     */ "B",
//         /* productType   */ "I",
//         /* exchange      */ "NSE",
//         /* tradingSymbol */ "RELIANCE-EQ",
//         /* quantity      */ 1,
//         /* priceType     */ "MKT");
//     std::cout << orderResp.dump(2) << std::endl;

//     // ── Example: Get order book ──
//     std::cout << "\n=== Order Book ===" << std::endl;
//     json orderBook = broker.getOrderBook();
//     std::cout << orderBook.dump(2) << std::endl;

//     // ── Example: Get positions ──
//     std::cout << "\n=== Positions ===" << std::endl;
//     json positions = broker.getPositionBook();
//     std::cout << positions.dump(2) << std::endl;

//     // ── Example: Get holdings ──
//     std::cout << "\n=== Holdings ===" << std::endl;
//     json holdings = broker.getHoldings();
//     std::cout << holdings.dump(2) << std::endl;

//     // ── Example: Get quotes ──
//     std::cout << "\n=== Quotes ===" << std::endl;
//     json quotes = broker.getQuotes("NSE", "22");
//     std::cout << quotes.dump(2) << std::endl;

//     // ── Example: Search scrip ──
//     std::cout << "\n=== Search ===" << std::endl;
//     json search = broker.searchScrip("NSE", "RELIANCE");
//     std::cout << search.dump(2) << std::endl;

//   } catch (const std::exception &e) {
//     std::cerr << "Error: " << e.what() << std::endl;
//     curl_global_cleanup();
//     return 1;
//   }

//   curl_global_cleanup();
//   return 0;
// }
