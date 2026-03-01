/**
 * Fyers Broker C++ Client (API V3)
 *
 * Based on the FYERS API V3 Postman collection.
 * Uses appId:access_token for Authorization header.
 *
 * Dependencies:
 *   - libcurl       (HTTP requests)
 *   - nlohmann/json (JSON library)
 *   - OpenSSL       (SHA-256 for appIdHash generation)
 *
 * Compile:
 *   g++ -std=c++17 -o fyers fyers.cpp -lcurl -lssl -lcrypto
 *   (MSVC: cl /std:c++17 /EHsc fyers.cpp libcurl.lib libssl.lib
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
#include <openssl/evp.h>
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
//  FyersBroker Class
// ──────────────────────────────────────────────

class FyersBroker {
public:
  // Order type constants (Fyers uses numeric types)
  static constexpr int ORDER_TYPE_LIMIT = 1;
  static constexpr int ORDER_TYPE_MARKET = 2;
  static constexpr int ORDER_TYPE_STOP = 3;
  static constexpr int ORDER_TYPE_STOP_LIMIT = 4;

  // Side constants
  static constexpr int SIDE_BUY = 1;
  static constexpr int SIDE_SELL = -1;

  // Position side
  static constexpr int POSITION_LONG = 1;
  static constexpr int POSITION_SHORT = -1;

  /**
   * Constructor
   *
   * @param appId        Application ID (client_id)
   * @param accessToken  Access token from auth flow
   */
  FyersBroker(const std::string &appId, const std::string &accessToken)
      : appId_(appId), accessToken_(accessToken) {
    baseUrl_ = "https://api-t1.fyers.in/api/v3";
    dataUrl_ = "https://api-t1.fyers.in/data";
    edisUrl_ = "https://api.fyers.in/api/v2";
    std::cout << "Fyers Broker initialized for appId: " << appId_ << std::endl;
  }

  // ─── Static: Auth Helpers ───────────────────

  /**
   * Generate appIdHash = SHA256(appId:secretKey)
   * Required for validate_auth_code step.
   */
  static std::string generateAppIdHash(const std::string &appId,
                                       const std::string &secretKey) {
    return sha256(appId + ":" + secretKey);
  }

  /**
   * Validate auth code and get access token.
   * Step 2 of the OIDC / OAuth2 auth flow.
   *
   * @param appIdHash  SHA256(appId:secretKey)
   * @param authCode   Authorization code from redirect
   * @return JSON with access_token etc.
   */
  static json validateAuthCode(const std::string &appIdHash,
                               const std::string &authCode) {
    json payload = {{"grant_type", "authorization_code"},
                    {"appIdHash", appIdHash},
                    {"code", authCode}};

    std::string url = "https://api-t1.fyers.in/api/v3/validate-authcode";

    CURL *curl = curl_easy_init();
    if (!curl)
      throw std::runtime_error("Failed to initialize libcurl");

    std::string body = payload.dump();
    std::string responseStr;

    struct curl_slist *headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");

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
      throw std::runtime_error(std::string("Auth validation failed: ") +
                               curl_easy_strerror(res));

    json response = json::parse(responseStr);
    std::cout << "validateAuthCode response: " << response.dump(2) << std::endl;
    return response;
  }

  // ─── Account Info ───────────────────────────

  /** Get user profile. */
  json getProfile() {
    json response = getRequest(baseUrl_ + "/profile");
    std::cout << "profile response: " << response.dump(2) << std::endl;
    return response;
  }

  /** Get funds / margin details. */
  json getFunds() {
    json response = getRequest(baseUrl_ + "/funds");
    std::cout << "funds response: " << response.dump(2) << std::endl;
    return response;
  }

  // ─── Transactions Info ──────────────────────

  /** Get trade book. */
  json getTradebook() {
    json response = getRequest(baseUrl_ + "/tradebook");
    std::cout << "tradebook response: " << response.dump(2) << std::endl;
    return response;
  }

  /** Get holdings. */
  json getHoldings() {
    json response = getRequest(baseUrl_ + "/holdings");
    std::cout << "holdings response: " << response.dump(2) << std::endl;
    return response;
  }

  /** Get positions. */
  json getPositions() {
    json response = getRequest(baseUrl_ + "/positions");
    std::cout << "positions response: " << response.dump(2) << std::endl;
    return response;
  }

  /** Get all orders. */
  json getOrders() {
    json response = getRequest(baseUrl_ + "/orders");
    std::cout << "orders response: " << response.dump(2) << std::endl;
    return response;
  }

  /** Get a specific order by ID. */
  json getOrder(const std::string &orderId) {
    json response = getRequest(baseUrl_ + "/orders?id=" + orderId);
    std::cout << "order response: " << response.dump(2) << std::endl;
    return response;
  }

  // ─── Orders ─────────────────────────────────

  /**
   * Place an order (single, sync).
   *
   * @param symbol       Symbol e.g. "NSE:SBIN-EQ"
   * @param qty          Quantity
   * @param type         Order type: 1=Limit, 2=Market, 3=Stop, 4=StopLimit
   * @param side         1=Buy, -1=Sell
   * @param productType  "INTRADAY", "CNC", "MARGIN", "CO", "BO"
   * @param limitPrice   Limit price (0 for market)
   * @param stopPrice    Stop price (0 if not stop order)
   * @param validity     "DAY", "IOC"
   * @param disclosedQty Disclosed quantity
   * @param offlineOrder After-market order flag
   * @param stopLoss     Stop-loss value (for CO/BO)
   * @param takeProfit   Take-profit value (for BO)
   */
  json placeOrder(const std::string &symbol, int qty, int type, int side,
                  const std::string &productType, double limitPrice = 0,
                  double stopPrice = 0, const std::string &validity = "DAY",
                  int disclosedQty = 0, bool offlineOrder = false,
                  double stopLoss = 0, double takeProfit = 0) {
    json payload = {{"symbol", symbol},
                    {"qty", qty},
                    {"type", type},
                    {"side", side},
                    {"productType", productType},
                    {"limitPrice", limitPrice},
                    {"stopPrice", stopPrice},
                    {"validity", validity},
                    {"disclosedQty", disclosedQty},
                    {"offlineOrder", offlineOrder},
                    {"stopLoss", stopLoss},
                    {"takeProfit", takeProfit}};

    json response = postJson(baseUrl_ + "/orders/sync", payload);
    std::cout << "placeOrder response: " << response.dump(2) << std::endl;
    return response;
  }

  /**
   * Modify an order (PATCH to /orders/sync).
   *
   * @param id         Order ID
   * @param type       New order type
   * @param qty        New quantity (0 to keep unchanged)
   * @param limitPrice New limit price (0 to keep unchanged)
   * @param stopPrice  New stop price (0 to keep unchanged)
   */
  json modifyOrder(const std::string &id, int type, int qty = 0,
                   double limitPrice = 0, double stopPrice = 0) {
    json payload = {{"id", id}, {"type", type}};
    if (qty > 0)
      payload["qty"] = qty;
    if (limitPrice > 0)
      payload["limitPrice"] = limitPrice;
    if (stopPrice > 0)
      payload["stopPrice"] = stopPrice;

    json response = patchJson(baseUrl_ + "/orders/sync", payload);
    std::cout << "modifyOrder response: " << response.dump(2) << std::endl;
    return response;
  }

  /**
   * Cancel an order.
   */
  json cancelOrder(const std::string &id) {
    json payload = {{"id", id}};
    json response = deleteWithBody(baseUrl_ + "/orders/sync", payload);
    std::cout << "cancelOrder response: " << response.dump(2) << std::endl;
    return response;
  }

  // ─── Multiple Orders ────────────────────────

  /**
   * Place multiple orders.
   * @param orders  JSON array of order objects
   */
  json placeMultipleOrders(const json &orders) {
    json response = postJsonRaw(baseUrl_ + "/multi-order/sync", orders.dump());
    std::cout << "placeMultipleOrders response: " << response.dump(2)
              << std::endl;
    return response;
  }

  /**
   * Modify multiple orders.
   * @param orders  JSON array of modify objects
   */
  json modifyMultipleOrders(const json &orders) {
    json response = patchJsonRaw(baseUrl_ + "/multi-order/sync", orders.dump());
    std::cout << "modifyMultipleOrders response: " << response.dump(2)
              << std::endl;
    return response;
  }

  /**
   * Cancel multiple orders.
   * @param orders  JSON array of {id: "..."} objects
   */
  json cancelMultipleOrders(const json &orders) {
    json response =
        deleteWithBodyRaw(baseUrl_ + "/multi-order/sync", orders.dump());
    std::cout << "cancelMultipleOrders response: " << response.dump(2)
              << std::endl;
    return response;
  }

  // ─── Position Management ────────────────────

  /**
   * Convert position product type.
   *
   * @param symbol        e.g. "NSE:SBIN-EQ"
   * @param positionSide  1 = Long, -1 = Short
   * @param convertQty    Quantity to convert
   * @param convertFrom   "INTRADAY", "CNC", "MARGIN"
   * @param convertTo     "INTRADAY", "CNC", "MARGIN"
   */
  json convertPosition(const std::string &symbol, int positionSide,
                       int convertQty, const std::string &convertFrom,
                       const std::string &convertTo) {
    json payload = {{"symbol", symbol},
                    {"positionSide", positionSide},
                    {"convertQty", convertQty},
                    {"convertFrom", convertFrom},
                    {"convertTo", convertTo}};

    json response = putJson(baseUrl_ + "/positions", payload);
    std::cout << "convertPosition response: " << response.dump(2) << std::endl;
    return response;
  }

  /**
   * Exit a specific position.
   * @param id  Position ID e.g. "NSE:SBIN-EQ-INTRADAY"
   */
  json exitPosition(const std::string &id) {
    json payload = {{"id", id}};
    json response = deleteWithBody(baseUrl_ + "/positions", payload);
    std::cout << "exitPosition response: " << response.dump(2) << std::endl;
    return response;
  }

  /** Exit all open positions. */
  json exitAllPositions() {
    json payload = {{"exit_all", 1}};
    json response = deleteWithBody(baseUrl_ + "/positions", payload);
    std::cout << "exitAllPositions response: " << response.dump(2) << std::endl;
    return response;
  }

  // ─── Market Data ────────────────────────────

  /**
   * Get quotes for symbols.
   * @param symbols  Comma-separated e.g. "NSE:SBIN-EQ,NSE:INFY-EQ"
   */
  json getQuotes(const std::string &symbols) {
    json response =
        getRequest(dataUrl_ + "/quotes/?symbols=" + urlEncode(symbols));
    std::cout << "quotes response: " << response.dump(2) << std::endl;
    return response;
  }

  /**
   * Get market depth (Level 2).
   * @param symbol    Single symbol e.g. "NSE:SBIN-EQ"
   * @param ohlcvFlag 1 = include OHLCV, 0 = bids/asks only
   */
  json getMarketDepth(const std::string &symbol, int ohlcvFlag = 1) {
    json response =
        getRequest(dataUrl_ + "/depth/?symbol=" + urlEncode(symbol) +
                   "&ohlcv_flag=" + std::to_string(ohlcvFlag));
    std::cout << "depth response: " << response.dump(2) << std::endl;
    return response;
  }

  /**
   * Get historical OHLCV data.
   *
   * @param symbol      e.g. "NSE:SBIN-EQ"
   * @param resolution  "1", "5", "15", "30", "60", "D" (day), "W", "M"
   * @param rangeFrom   Start date "YYYY-MM-DD"
   * @param rangeTo     End date "YYYY-MM-DD"
   * @param dateFormat  1 = string dates, 0 = epoch
   * @param contFlag    1 = continuous data for futures
   */
  json getHistoricalData(const std::string &symbol,
                         const std::string &resolution,
                         const std::string &rangeFrom,
                         const std::string &rangeTo, int dateFormat = 1,
                         int contFlag = 1) {
    std::string url = dataUrl_ + "/history?symbol=" + urlEncode(symbol) +
                      "&resolution=" + resolution +
                      "&date_format=" + std::to_string(dateFormat) +
                      "&range_from=" + rangeFrom + "&range_to=" + rangeTo +
                      "&cont_flag=" + std::to_string(contFlag);
    json response = getRequest(url);
    std::cout << "historical response: " << response.dump(2) << std::endl;
    return response;
  }

  // ─── Broker Info ────────────────────────────

  /** Get market status (open/close per segment). */
  json getMarketStatus() {
    json response = getRequest(dataUrl_ + "/marketStatus");
    std::cout << "marketStatus response: " << response.dump(2) << std::endl;
    return response;
  }

  // ─── EDIS (e-DIS) ──────────────────────────

  /** Generate TPIN for CDSL e-DIS authorization. */
  json generateTpin() {
    json response = getRequest(edisUrl_ + "/tpin");
    std::cout << "tpin response: " << response.dump(2) << std::endl;
    return response;
  }

  /** Get EDIS details. */
  json getEdisDetails() {
    json response = getRequest(edisUrl_ + "/details");
    std::cout << "edis details response: " << response.dump(2) << std::endl;
    return response;
  }

  /**
   * Submit EDIS holdings for authorization.
   * @param recordList  JSON array of {isin_code, qty}
   */
  json submitEdisHoldings(const json &recordList) {
    json payload = {{"recordLst", recordList}};
    json response = postJson(edisUrl_ + "/index", payload);
    std::cout << "edis index response: " << response.dump(2) << std::endl;
    return response;
  }

  /** Inquire EDIS transaction status. */
  json inquireEdis(const std::string &transactionId) {
    json payload = {{"transactionId", transactionId}};
    json response = postJson(edisUrl_ + "/inquiry", payload);
    std::cout << "edis inquiry response: " << response.dump(2) << std::endl;
    return response;
  }

  // ─── Getters / Setters ──────────────────────

  std::string getAppId() const { return appId_; }
  std::string getAccessToken() const { return accessToken_; }

  void setAccessToken(const std::string &token) { accessToken_ = token; }

private:
  std::string appId_;
  std::string accessToken_;
  std::string baseUrl_;
  std::string dataUrl_;
  std::string edisUrl_;

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

  /** Build Fyers auth headers: Authorization: appId:access_token */
  struct curl_slist *buildHeaders(bool isJson = true) {
    struct curl_slist *headers = nullptr;
    std::string auth = "Authorization: " + appId_ + ":" + accessToken_;
    headers = curl_slist_append(headers, auth.c_str());

    if (isJson)
      headers = curl_slist_append(headers, "Content-Type: application/json");

    headers = curl_slist_append(headers, "version: 3");
    return headers;
  }

  /** Parse Fyers response. */
  json parseResponse(const std::string &responseStr) {
    try {
      return json::parse(responseStr);
    } catch (const json::parse_error &e) {
      std::cerr << "JSON parse error: " << e.what() << std::endl;
      std::cerr << "Raw response: " << responseStr << std::endl;
      throw;
    }
  }

  /** GET request. */
  json getRequest(const std::string &url) {
    CURL *curl = curl_easy_init();
    if (!curl)
      throw std::runtime_error("Failed to initialize libcurl");

    std::string responseStr;
    struct curl_slist *headers = buildHeaders(false);

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

    return parseResponse(responseStr);
  }

  /** POST JSON body. */
  json postJson(const std::string &url, const json &payload) {
    return postJsonRaw(url, payload.dump());
  }

  /** POST raw JSON string. */
  json postJsonRaw(const std::string &url, const std::string &body) {
    CURL *curl = curl_easy_init();
    if (!curl)
      throw std::runtime_error("Failed to initialize libcurl");

    std::string responseStr;
    struct curl_slist *headers = buildHeaders(true);

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

  /** PATCH JSON body (used for modify order). */
  json patchJson(const std::string &url, const json &payload) {
    return patchJsonRaw(url, payload.dump());
  }

  json patchJsonRaw(const std::string &url, const std::string &body) {
    CURL *curl = curl_easy_init();
    if (!curl)
      throw std::runtime_error("Failed to initialize libcurl");

    std::string responseStr;
    struct curl_slist *headers = buildHeaders(true);

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PATCH");
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &responseStr);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);

    CURLcode res = curl_easy_perform(curl);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK)
      throw std::runtime_error(std::string("HTTP PATCH failed: ") +
                               curl_easy_strerror(res));

    return parseResponse(responseStr);
  }

  /** PUT JSON body (used for position conversion). */
  json putJson(const std::string &url, const json &payload) {
    std::string body = payload.dump();

    CURL *curl = curl_easy_init();
    if (!curl)
      throw std::runtime_error("Failed to initialize libcurl");

    std::string responseStr;
    struct curl_slist *headers = buildHeaders(true);

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PUT");
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &responseStr);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);

    CURLcode res = curl_easy_perform(curl);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK)
      throw std::runtime_error(std::string("HTTP PUT failed: ") +
                               curl_easy_strerror(res));

    return parseResponse(responseStr);
  }

  /** DELETE with JSON body (Fyers uses body in DELETE requests). */
  json deleteWithBody(const std::string &url, const json &payload) {
    return deleteWithBodyRaw(url, payload.dump());
  }

  json deleteWithBodyRaw(const std::string &url, const std::string &body) {
    CURL *curl = curl_easy_init();
    if (!curl)
      throw std::runtime_error("Failed to initialize libcurl");

    std::string responseStr;
    struct curl_slist *headers = buildHeaders(true);

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "DELETE");
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &responseStr);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);

    CURLcode res = curl_easy_perform(curl);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK)
      throw std::runtime_error(std::string("HTTP DELETE failed: ") +
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
//     // Replace with your actual Fyers credentials
//     std::string appId = "YOUR_APP_ID";
//     std::string accessToken = "YOUR_ACCESS_TOKEN";

//     FyersBroker broker(appId, accessToken);

//     // ── Example: Get profile ──
//     std::cout << "\n=== Profile ===" << std::endl;
//     json profile = broker.getProfile();
//     std::cout << profile.dump(2) << std::endl;

//     // ── Example: Get funds ──
//     std::cout << "\n=== Funds ===" << std::endl;
//     json funds = broker.getFunds();
//     std::cout << funds.dump(2) << std::endl;

//     // ── Example: Place a market order ──
//     std::cout << "\n=== Place Order ===" << std::endl;
//     json orderResp = broker.placeOrder(
//         /* symbol      */ "NSE:SBIN-EQ",
//         /* qty         */ 1,
//         /* type        */ FyersBroker::ORDER_TYPE_MARKET,
//         /* side        */ FyersBroker::SIDE_BUY,
//         /* productType */ "CNC");
//     std::cout << orderResp.dump(2) << std::endl;

//     // ── Example: Get orders ──
//     std::cout << "\n=== Orders ===" << std::endl;
//     json orders = broker.getOrders();
//     std::cout << orders.dump(2) << std::endl;

//     // ── Example: Get positions ──
//     std::cout << "\n=== Positions ===" << std::endl;
//     json positions = broker.getPositions();
//     std::cout << positions.dump(2) << std::endl;

//     // ── Example: Get holdings ──
//     std::cout << "\n=== Holdings ===" << std::endl;
//     json holdings = broker.getHoldings();
//     std::cout << holdings.dump(2) << std::endl;

//     // ── Example: Get quotes ──
//     std::cout << "\n=== Quotes ===" << std::endl;
//     json quotes = broker.getQuotes("NSE:SBIN-EQ");
//     std::cout << quotes.dump(2) << std::endl;

//     // ── Example: Get historical data ──
//     std::cout << "\n=== Historical ===" << std::endl;
//     json hist = broker.getHistoricalData("NSE:SBIN-EQ", "30", "2025-01-01",
//                                          "2025-01-31");
//     std::cout << hist.dump(2) << std::endl;

//   } catch (const std::exception &e) {
//     std::cerr << "Error: " << e.what() << std::endl;
//     curl_global_cleanup();
//     return 1;
//   }

//   curl_global_cleanup();
//   return 0;
// }
