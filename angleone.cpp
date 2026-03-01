/**
 * Angel One (SmartConnect) C++ Client
 *
 * Converted from SmartApi/smartConnect.py — the official Angel One
 * SmartConnect API wrapper.
 *
 * Dependencies:
 *   - libcurl       (HTTP requests)
 *   - nlohmann/json (JSON library)
 *
 * Compile:
 *   g++ -std=c++17 -o angelone angelone.cpp -lcurl
 *   (MSVC: cl /std:c++17 /EHsc angelone.cpp libcurl.lib)
 */

#include <algorithm>
#include <cstring>
#include <iostream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include <curl/curl.h>

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
//  AngelOneBroker Class
// ──────────────────────────────────────────────

class AngelOneBroker {
public:
  /**
   * Constructor
   *
   * @param apiKey       Your Angel One API key (X-PrivateKey)
   * @param accessToken  Pre-obtained JWT access token
   * @param clientCode   Client code / user ID
   */
  AngelOneBroker(const std::string &apiKey, const std::string &accessToken,
                 const std::string &clientCode)
      : apiKey_(apiKey), accessToken_(accessToken), clientCode_(clientCode) {
    rootUrl_ = "https://apiconnect.angelone.in";
    std::cout << "Angel One Broker initialized." << std::endl;
  }

  // ─── User Profile ───────────────────────────

  /** Get user profile. */
  json getProfile() {
    json response = getRequest("/rest/secure/angelbroking/user/v1/getProfile");
    std::cout << "getProfile response: " << response.dump(2) << std::endl;
    return response;
  }

  /** Get RMS limit (funds/margin). */
  json getRmsLimit() {
    json response = getRequest("/rest/secure/angelbroking/user/v1/getRMS");
    std::cout << "getRmsLimit response: " << response.dump(2) << std::endl;
    return response;
  }

  // ─── Orders ─────────────────────────────────

  /**
   * Place an order.
   *
   * Order params (all as JSON keys):
   *   variety        : "NORMAL", "STOPLOSS", "AMO", "ROBO"
   *   tradingsymbol  : e.g. "SBIN-EQ"
   *   symboltoken    : e.g. "3045"
   *   transactiontype: "BUY" or "SELL"
   *   exchange       : "NSE", "BSE", "NFO", "MCX", "CDS", "BFO"
   *   ordertype      : "MARKET", "LIMIT", "STOPLOSS_LIMIT", "STOPLOSS_MARKET"
   *   producttype    : "DELIVERY", "CARRYFORWARD", "MARGIN", "INTRADAY", "BO"
   *   duration       : "DAY", "IOC"
   *   price          : limit price (string)
   *   squareoff      : square-off value (for BO)
   *   stoploss       : stop-loss value (for BO)
   *   quantity       : order quantity (string)
   *   triggerprice   : trigger price (for SL orders)
   */
  json placeOrder(const json &orderParams) {
    json response = postRequest("/rest/secure/angelbroking/order/v1/placeOrder",
                                orderParams);
    std::cout << "placeOrder response: " << response.dump(2) << std::endl;
    return response;
  }

  /** Place order and return full response (not just orderid). */
  json placeOrderFullResponse(const json &orderParams) {
    json response = postRequest("/rest/secure/angelbroking/order/v1/placeOrder",
                                orderParams);
    std::cout << "placeOrderFullResponse: " << response.dump(2) << std::endl;
    return response;
  }

  /** Modify an existing order. */
  json modifyOrder(const json &orderParams) {
    json response = postRequest(
        "/rest/secure/angelbroking/order/v1/modifyOrder", orderParams);
    std::cout << "modifyOrder response: " << response.dump(2) << std::endl;
    return response;
  }

  /** Cancel an order. */
  json cancelOrder(const std::string &orderId, const std::string &variety) {
    json payload = {{"variety", variety}, {"orderid", orderId}};
    json response =
        postRequest("/rest/secure/angelbroking/order/v1/cancelOrder", payload);
    std::cout << "cancelOrder response: " << response.dump(2) << std::endl;
    return response;
  }

  /** Get order book. */
  json getOrderBook() {
    json response =
        getRequest("/rest/secure/angelbroking/order/v1/getOrderBook");
    std::cout << "orderBook response: " << response.dump(2) << std::endl;
    return response;
  }

  /** Get trade book. */
  json getTradeBook() {
    json response =
        getRequest("/rest/secure/angelbroking/order/v1/getTradeBook");
    std::cout << "tradeBook response: " << response.dump(2) << std::endl;
    return response;
  }

  /** Get individual order details. */
  json getOrderDetails(const std::string &orderId) {
    std::string url =
        rootUrl_ + "/rest/secure/angelbroking/order/v1/details/" + orderId;

    CURL *curl = curl_easy_init();
    if (!curl)
      throw std::runtime_error("Failed to initialize libcurl");

    std::string responseStr;
    struct curl_slist *headers = buildHeaders();

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &responseStr);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);

    CURLcode res = curl_easy_perform(curl);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK)
      throw std::runtime_error(std::string("HTTP GET failed: ") +
                               curl_easy_strerror(res));

    json response = parseResponse(responseStr);
    std::cout << "orderDetails response: " << response.dump(2) << std::endl;
    return response;
  }

  // ─── Portfolio ──────────────────────────────

  /** Get holdings. */
  json getHoldings() {
    json response =
        getRequest("/rest/secure/angelbroking/portfolio/v1/getHolding");
    std::cout << "holdings response: " << response.dump(2) << std::endl;
    return response;
  }

  /** Get all holdings (including T1). */
  json getAllHoldings() {
    json response =
        getRequest("/rest/secure/angelbroking/portfolio/v1/getAllHolding");
    std::cout << "allHoldings response: " << response.dump(2) << std::endl;
    return response;
  }

  /** Get positions. */
  json getPositions() {
    json response =
        getRequest("/rest/secure/angelbroking/order/v1/getPosition");
    std::cout << "positions response: " << response.dump(2) << std::endl;
    return response;
  }

  /** Convert position product type. */
  json convertPosition(const json &positionParams) {
    json response = postRequest(
        "/rest/secure/angelbroking/order/v1/convertPosition", positionParams);
    std::cout << "convertPosition response: " << response.dump(2) << std::endl;
    return response;
  }

  // ─── Market Data ────────────────────────────

  /** Get LTP data for a symbol. */
  json getLtpData(const std::string &exchange, const std::string &tradingSymbol,
                  const std::string &symbolToken) {
    json payload = {{"exchange", exchange},
                    {"tradingsymbol", tradingSymbol},
                    {"symboltoken", symbolToken}};
    json response =
        postRequest("/rest/secure/angelbroking/order/v1/getLtpData", payload);
    std::cout << "ltpData response: " << response.dump(2) << std::endl;
    return response;
  }

  /**
   * Get market data (quotes).
   * @param mode           "FULL", "LTP", "OHLC"
   * @param exchangeTokens Map of exchange -> vector of tokens
   *                       e.g. {{"NSE", {"3045", "99926000"}}}
   */
  json getMarketData(const std::string &mode, const json &exchangeTokens) {
    json payload = {{"mode", mode}, {"exchangeTokens", exchangeTokens}};
    json response =
        postRequest("/rest/secure/angelbroking/market/v1/quote", payload);
    std::cout << "marketData response: " << response.dump(2) << std::endl;
    return response;
  }

  /** Get historical candle data. */
  json getCandleData(const json &historicDataParams) {
    json response =
        postRequest("/rest/secure/angelbroking/historical/v1/getCandleData",
                    historicDataParams);
    std::cout << "candleData response: " << response.dump(2) << std::endl;
    return response;
  }

  /** Get Open Interest data. */
  json getOIData(const json &historicOIParams) {
    json response = postRequest(
        "/rest/secure/angelbroking/historical/v1/getOIData", historicOIParams);
    std::cout << "oiData response: " << response.dump(2) << std::endl;
    return response;
  }

  /** Search for a scrip/instrument. */
  json searchScrip(const std::string &exchange,
                   const std::string &searchScrip) {
    json payload = {{"exchange", exchange}, {"searchscrip", searchScrip}};
    json response =
        postRequest("/rest/secure/angelbroking/order/v1/searchScrip", payload);
    std::cout << "searchScrip response: " << response.dump(2) << std::endl;
    return response;
  }

  // ─── GTT Orders ─────────────────────────────

  /** Create a GTT rule. */
  json gttCreateRule(const json &createRuleParams) {
    json response =
        postRequest("/gtt-service/rest/secure/angelbroking/gtt/v1/createRule",
                    createRuleParams);
    std::cout << "gttCreate response: " << response.dump(2) << std::endl;
    return response;
  }

  /** Modify a GTT rule. */
  json gttModifyRule(const json &modifyRuleParams) {
    json response =
        postRequest("/gtt-service/rest/secure/angelbroking/gtt/v1/modifyRule",
                    modifyRuleParams);
    std::cout << "gttModify response: " << response.dump(2) << std::endl;
    return response;
  }

  /** Cancel a GTT rule. */
  json gttCancelRule(const json &cancelParams) {
    json response = postRequest("/rest/secure/angelbroking/gtt/v1/cancelRule",
                                cancelParams);
    std::cout << "gttCancel response: " << response.dump(2) << std::endl;
    return response;
  }

  /** Get GTT rule details by ID. */
  json gttDetails(const std::string &id) {
    json payload = {{"id", id}};
    json response =
        postRequest("/rest/secure/angelbroking/gtt/v1/ruleDetails", payload);
    std::cout << "gttDetails response: " << response.dump(2) << std::endl;
    return response;
  }

  /** List GTT rules. */
  json gttList(const json &statusList, int page, int count) {
    json payload = {{"status", statusList}, {"page", page}, {"count", count}};
    json response =
        postRequest("/rest/secure/angelbroking/gtt/v1/ruleList", payload);
    std::cout << "gttList response: " << response.dump(2) << std::endl;
    return response;
  }

  // ─── Margin ─────────────────────────────────

  /** Get margin details. */
  json getMarginApi(const json &params) {
    json response =
        postRequest("rest/secure/angelbroking/margin/v1/batch", params);
    std::cout << "margin response: " << response.dump(2) << std::endl;
    return response;
  }

  /** Estimate brokerage charges. */
  json estimateCharges(const json &params) {
    json response = postRequest(
        "rest/secure/angelbroking/brokerage/v1/estimateCharges", params);
    std::cout << "estimateCharges response: " << response.dump(2) << std::endl;
    return response;
  }

  // ─── EDIS ───────────────────────────────────

  /** Generate TPIN for EDIS. */
  json generateTPIN(const json &params) {
    json response =
        postRequest("rest/secure/angelbroking/edis/v1/generateTPIN", params);
    std::cout << "generateTPIN response: " << response.dump(2) << std::endl;
    return response;
  }

  /** Verify DIS. */
  json verifyDis(const json &params) {
    json response =
        postRequest("rest/secure/angelbroking/edis/v1/verifyDis", params);
    std::cout << "verifyDis response: " << response.dump(2) << std::endl;
    return response;
  }

  /** Get EDIS transaction status. */
  json getTranStatus(const json &params) {
    json response =
        postRequest("rest/secure/angelbroking/edis/v1/getTranStatus", params);
    std::cout << "getTranStatus response: " << response.dump(2) << std::endl;
    return response;
  }

  // ─── Market Analytics ───────────────────────

  /** Get option greeks. */
  json optionGreek(const json &params) {
    json response = postRequest(
        "rest/secure/angelbroking/marketData/v1/optionGreek", params);
    std::cout << "optionGreek response: " << response.dump(2) << std::endl;
    return response;
  }

  /** Get gainers & losers. */
  json gainersLosers(const json &params) {
    json response = postRequest(
        "rest/secure/angelbroking/marketData/v1/gainersLosers", params);
    std::cout << "gainersLosers response: " << response.dump(2) << std::endl;
    return response;
  }

  /** Get put-call ratio. */
  json putCallRatio() {
    json response =
        getRequest("rest/secure/angelbroking/marketData/v1/putCallRatio");
    std::cout << "putCallRatio response: " << response.dump(2) << std::endl;
    return response;
  }

  /** Get OI buildup. */
  json oiBuildup(const json &params) {
    json response =
        postRequest("rest/secure/angelbroking/marketData/v1/OIBuildup", params);
    std::cout << "oiBuildup response: " << response.dump(2) << std::endl;
    return response;
  }

  /** Get NSE intraday data. */
  json nseIntraday() {
    json response =
        getRequest("rest/secure/angelbroking/marketData/v1/nseIntraday");
    std::cout << "nseIntraday response: " << response.dump(2) << std::endl;
    return response;
  }

  /** Get BSE intraday data. */
  json bseIntraday() {
    json response =
        getRequest("rest/secure/angelbroking/marketData/v1/bseIntraday");
    std::cout << "bseIntraday response: " << response.dump(2) << std::endl;
    return response;
  }

  // ─── Getters / Setters ──────────────────────

  std::string getAccessToken() const { return accessToken_; }
  std::string getClientCode() const { return clientCode_; }

  void setAccessToken(const std::string &token) { accessToken_ = token; }
  void setClientCode(const std::string &code) { clientCode_ = code; }

private:
  std::string apiKey_;
  std::string accessToken_;
  std::string clientCode_;
  std::string rootUrl_;

  // Client metadata (matching Python SmartConnect)
  std::string clientLocalIP_ = "127.0.0.1";
  std::string clientPublicIP_ = "106.193.147.98";
  std::string clientMACAddress_ = "fe:49:e2:0f:3c:95";
  std::string userType_ = "USER";
  std::string sourceID_ = "WEB";

  // ─── HTTP Helpers ───────────────────────────

  /**
   * Build Angel One specific headers.
   * Matches Python requestHeaders() exactly.
   */
  struct curl_slist *buildHeaders() {
    struct curl_slist *headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    headers = curl_slist_append(headers, "Accept: application/json");

    std::string localIP = "X-ClientLocalIP: " + clientLocalIP_;
    headers = curl_slist_append(headers, localIP.c_str());

    std::string publicIP = "X-ClientPublicIP: " + clientPublicIP_;
    headers = curl_slist_append(headers, publicIP.c_str());

    std::string mac = "X-MACAddress: " + clientMACAddress_;
    headers = curl_slist_append(headers, mac.c_str());

    std::string pk = "X-PrivateKey: " + apiKey_;
    headers = curl_slist_append(headers, pk.c_str());

    std::string ut = "X-UserType: " + userType_;
    headers = curl_slist_append(headers, ut.c_str());

    std::string sid = "X-SourceID: " + sourceID_;
    headers = curl_slist_append(headers, sid.c_str());

    if (!accessToken_.empty()) {
      std::string auth = "Authorization: Bearer " + accessToken_;
      headers = curl_slist_append(headers, auth.c_str());
    }

    return headers;
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

  /** POST JSON request. */
  json postRequest(const std::string &path, const json &payload) {
    std::string url = rootUrl_ + "/" + path;
    // Avoid double slashes
    if (path.front() == '/')
      url = rootUrl_ + path;

    std::string body = payload.dump();

    CURL *curl = curl_easy_init();
    if (!curl)
      throw std::runtime_error("Failed to initialize libcurl");

    std::string responseStr;
    struct curl_slist *headers = buildHeaders();

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &responseStr);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 7L);

    CURLcode res = curl_easy_perform(curl);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK)
      throw std::runtime_error(std::string("HTTP POST failed: ") +
                               curl_easy_strerror(res));

    return parseResponse(responseStr);
  }

  /** GET request (some endpoints accept optional JSON params as query). */
  json getRequest(const std::string &path, const json &params = {}) {
    std::string url = rootUrl_ + "/" + path;
    if (path.front() == '/')
      url = rootUrl_ + path;

    CURL *curl = curl_easy_init();
    if (!curl)
      throw std::runtime_error("Failed to initialize libcurl");

    std::string responseStr;
    struct curl_slist *headers = buildHeaders();

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &responseStr);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 7L);

    CURLcode res = curl_easy_perform(curl);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK)
      throw std::runtime_error(std::string("HTTP GET failed: ") +
                               curl_easy_strerror(res));

    return parseResponse(responseStr);
  }
};

// ──────────────────────────────────────────────
//  main() – Usage demo
// ──────────────────────────────────────────────

// int main() {
//   curl_global_init(CURL_GLOBAL_DEFAULT);

//   try {
//     // Replace with your actual Angel One credentials
//     std::string apiKey = "FZLDMjMp";
//     std::string accessToken =
//         "eyJhbGciOiJIUzUxMiJ9.eyJ1c2VybmFtZSI6IkoyNTI1MDUiLCJyb2xlcyI6MCwidXNlcnR5cGUiOiJVU0VSIiwidG9rZW4iOiJleUpoYkdjaU9pSlNVekkxTmlJc0luUjVjQ0k2SWtwWFZDSjkuZXlKMWMyVnlYM1I1Y0dVaU9pSmpiR2xsYm5RaUxDSjBiMnRsYmw5MGVYQmxJam9pZEhKaFpHVmZZV05qWlhOelgzUnZhMlZ1SWl3aVoyMWZhV1FpT2pFekxDSnpiM1Z5WTJVaU9pSXpJaXdpWkdWMmFXTmxYMmxrSWpvaVkyRTRaakJoTkdJdFl6QmtPQzB6T0RZNUxUZzRNVFl0T0RBNU1EQTJaRFZpTnpZMklpd2lhMmxrSWpvaWRISmhaR1ZmYTJWNVgzWXlJaXdpYjIxdVpXMWhibUZuWlhKcFpDSTZNVE1zSW5CeWIyUjFZM1J6SWpwN0ltUmxiV0YwSWpwN0luTjBZWFIxY3lJNkltRmpkR2wyWlNKOUxDSnRaaUk2ZXlKemRHRjBkWE1pT2lKaFkzUnBkbVVpZlgwc0ltbHpjeUk2SW5SeVlXUmxYMnh2WjJsdVgzTmxjblpwWTJVaUxDSnpkV0lpT2lKS01qVXlOVEExSWl3aVpYaHdJam94TnpjeU5ETTJNakUzTENKdVltWWlPakUzTnpJek5EazJNemNzSW1saGRDSTZNVGMzTWpNME9UWXpOeXdpYW5ScElqb2lNbUpsTVRsbFpqWXROVEkxT1MwME5UQXpMVGxoTVRFdFkyWmhPREEzTXpRelpEY3hJaXdpVkc5clpXNGlPaUlpZlEuZGtqQWZlOHVCRXhOeDRNN2FDNTg0X1FSV1h3SkhnQWwwNzhXNTlLa1FVTldDdm0zdy0xMHdOZW1IMEtpRjFHeklES3hPX0hTa1lCN3U5QkQ4UmZORFZleWZSWHRYQnBnNkpJRWx0NExFbHFXX3JFQ1ZjSVI0NnNJSGhqM3pPVUM0aGZWRVdjN3hkakVmVW53bVZiUEdOUnlsMkkzeDJ4VjRJVnlmaVVyVkRBIiwiQVBJLUtFWSI6IkZaTERNak1wIiwiWC1PTEQtQVBJLUtFWSI6dHJ1ZSwiaWF0IjoxNzcyMzQ5ODE3LCJleHAiOjE3NzIzODk4MDB9.8_NaN5qtNu5Iui24k2EqCeU9UjjpLmynPPrHe2YwAYkp9nAktIG-2F6ZipaNbiv62fdQhuBD9rqLyeQpWbXpsg"; // Provide your access token directly
//     std::string clientCode = "J252505";

//     AngelOneBroker broker(apiKey, accessToken, clientCode);

//     // ── Example: Get profile ──
//     std::cout << "\n=== Profile ===" << std::endl;
//     json profile = broker.getProfile();
//     std::cout << profile.dump(2) << std::endl;

//     // ── Example: Get RMS/funds ──
//     std::cout << "\n=== RMS Limit ===" << std::endl;
//     json rms = broker.getRmsLimit();
//     std::cout << rms.dump(2) << std::endl;

//     // ── Example: Place order ──
//     std::cout << "\n=== Place Order ===" << std::endl;
//     json orderParams = {{"variety", "NORMAL"},
//                         {"tradingsymbol", "NIFTY30JUN26C30000"},
//                         {"symboltoken", "35229"},
//                         {"transactiontype", "BUY"},
//                         {"exchange", "NFO"},
//                         {"ordertype", "MARKET"},
//                         {"producttype", "INTRADAY"},
//                         {"duration", "DAY"},
//                         {"price", "0"},
//                         {"quantity", "65"}};
//     json orderResp = broker.placeOrder(orderParams);
//     std::cout << orderResp.dump(2) << std::endl;

//     // ── Example: Get order book ──
//     std::cout << "\n=== Order Book ===" << std::endl;
//     json orderBook = broker.getOrderBook();
//     std::cout << orderBook.dump(2) << std::endl;

//     // ── Example: Get positions ──
//     std::cout << "\n=== Positions ===" << std::endl;
//     json positions = broker.getPositions();
//     std::cout << positions.dump(2) << std::endl;

//     // ── Example: Get holdings ──
//     std::cout << "\n=== Holdings ===" << std::endl;
//     json holdings = broker.getHoldings();
//     std::cout << holdings.dump(2) << std::endl;

//     // ── Example: Get LTP ──
//     std::cout << "\n=== LTP ===" << std::endl;
//     json ltp = broker.getLtpData("NSE", "SBIN-EQ", "3045");
//     std::cout << ltp.dump(2) << std::endl;

//   } catch (const std::exception &e) {
//     std::cerr << "Error: " << e.what() << std::endl;
//     curl_global_cleanup();
//     return 1;
//   }

//   curl_global_cleanup();
//   return 0;
// }
