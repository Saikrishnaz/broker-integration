/**
 * Upstox Broker C++ Client
 *
 * Based on the Upstox Developer API v2 (Postman collection reference).
 * Uses Bearer token authentication with Api-Version header.
 *
 * Dependencies:
 *   - libcurl       (HTTP requests)
 *   - nlohmann/json (json.hpp, single-header JSON library)
 *
 * Compile:
 *   g++ -std=c++17 -o upstox upstox.cpp -lcurl
 *   (MSVC: cl /std:c++17 /EHsc upstox.cpp libcurl.lib)
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
//  UpstoxBroker Class
// ──────────────────────────────────────────────

class UpstoxBroker {
public:
  /**
   * Constructor
   *
   * @param accessToken  Bearer token obtained from Upstox OAuth flow
   * @param apiVersion   API version (default "2.0")
   */
  explicit UpstoxBroker(const std::string &accessToken,
                        const std::string &apiVersion = "2.0")
      : accessToken_(accessToken), apiVersion_(apiVersion) {
    baseUrl_ = "https://api.upstox.com/v2";
    std::cout << "Upstox Broker initialized." << std::endl;
  }

  // ─── Static: Token Exchange ─────────────────

  /**
   * Exchange authorization code for access token.
   * This is a static method — no access token needed yet.
   *
   * @param clientId     OAuth API key
   * @param clientSecret OAuth client secret
   * @param code         Authorization code from redirect
   * @param redirectUri  Redirect URI used during login
   * @return JSON with access_token, user profile, etc.
   */
  static json getAccessToken(const std::string &clientId,
                             const std::string &clientSecret,
                             const std::string &code,
                             const std::string &redirectUri) {
    std::string url = "https://api.upstox.com/v2/login/authorization/token";

    CURL *curl = curl_easy_init();
    if (!curl)
      throw std::runtime_error("Failed to initialize libcurl");

    // Build form body
    std::string body = "client_id=" + urlEncode(clientId) +
                       "&client_secret=" + urlEncode(clientSecret) +
                       "&code=" + urlEncode(code) +
                       "&grant_type=authorization_code" +
                       "&redirect_uri=" + urlEncode(redirectUri);

    std::string responseStr;
    struct curl_slist *headers = nullptr;
    headers = curl_slist_append(headers, "Api-Version: 2.0");
    headers = curl_slist_append(headers, "Accept: application/json");
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
      throw std::runtime_error(std::string("Token exchange failed: ") +
                               curl_easy_strerror(res));

    json response = json::parse(responseStr);
    std::cout << "Token response: " << response.dump(2) << std::endl;
    return response;
  }

  // ─── User ───────────────────────────────────

  /** Get user profile. */
  json getProfile() { return getRequest("/user/profile"); }

  /** Get fund and margin details. Optionally pass segment. */
  json getFundsAndMargin(const std::string &segment = "") {
    if (!segment.empty())
      return getRequest("/user/get-funds-and-margin?segment=" + segment);
    return getRequest("/user/get-funds-and-margin");
  }

  // ─── Orders ─────────────────────────────────

  /**
   * Place an order.
   *
   * @param instrumentToken  Instrument token (e.g. "NSE_EQ|INE669E01016")
   * @param quantity         Order quantity
   * @param product          "D" (Delivery), "I" (Intraday), "CO", "OCO"
   * @param validity         "DAY", "IOC"
   * @param orderType        "MARKET", "LIMIT", "SL", "SL-M"
   * @param transactionType  "BUY" or "SELL"
   * @param price            Limit price (0 for market)
   * @param triggerPrice     Trigger price for SL orders (0 if not SL)
   * @param disclosedQty     Disclosed quantity (0 for full)
   * @param isAmo            After-market order (default false)
   * @param tag              Order tag (optional)
   */
  json placeOrder(const std::string &instrumentToken, int quantity,
                  const std::string &product, const std::string &validity,
                  const std::string &orderType,
                  const std::string &transactionType, double price = 0,
                  double triggerPrice = 0, int disclosedQty = 0,
                  bool isAmo = false, const std::string &tag = "") {
    json payload = {{"instrument_token", instrumentToken},
                    {"quantity", quantity},
                    {"product", product},
                    {"validity", validity},
                    {"order_type", orderType},
                    {"transaction_type", transactionType},
                    {"price", price},
                    {"trigger_price", triggerPrice},
                    {"disclosed_quantity", disclosedQty},
                    {"is_amo", isAmo}};

    if (!tag.empty())
      payload["tag"] = tag;

    json response = postJson("/order/place", payload);
    std::cout << "placeOrder response: " << response.dump(2) << std::endl;
    return response;
  }

  /**
   * Modify an existing order.
   */
  json modifyOrder(const std::string &orderId, int quantity,
                   const std::string &validity, const std::string &orderType,
                   double price = 0, double triggerPrice = 0,
                   int disclosedQty = 0) {
    json payload = {{"order_id", orderId},
                    {"quantity", quantity},
                    {"validity", validity},
                    {"order_type", orderType},
                    {"price", price},
                    {"trigger_price", triggerPrice},
                    {"disclosed_quantity", disclosedQty}};

    json response = putJson("/order/modify", payload);
    std::cout << "modifyOrder response: " << response.dump(2) << std::endl;
    return response;
  }

  /**
   * Cancel an order.
   */
  json cancelOrder(const std::string &orderId) {
    json response = deleteRequest("/order/cancel?order_id=" + orderId);
    std::cout << "cancelOrder response: " << response.dump(2) << std::endl;
    return response;
  }

  /** Get order book (all orders for the day). */
  json getOrderBook() { return getRequest("/order/retrieve-all"); }

  /** Get order details for a specific order. */
  json getOrderDetails(const std::string &orderId) {
    return getRequest("/order/history?order_id=" + orderId);
  }

  /** Get trade book (all trades). */
  json getTradeBook() { return getRequest("/order/trades/get-trades-for-day"); }

  /** Get trades for a specific order. */
  json getTradesForOrder(const std::string &orderId) {
    return getRequest("/order/trades?order_id=" + orderId);
  }

  // ─── Portfolio ──────────────────────────────

  /** Get positions. */
  json getPositions() { return getRequest("/portfolio/short-term-positions"); }

  /** Get long-term positions (holdings). */
  json getLongTermPositions() {
    return getRequest("/portfolio/long-term-holdings");
  }

  /** Get holdings. */
  json getHoldings() { return getRequest("/portfolio/long-term-holdings"); }

  /**
   * Convert position product type.
   */
  json convertPosition(const std::string &instrumentToken,
                       const std::string &transactionType, int quantity,
                       const std::string &oldProduct,
                       const std::string &newProduct) {
    json payload = {{"instrument_token", instrumentToken},
                    {"transaction_type", transactionType},
                    {"quantity", quantity},
                    {"old_product", oldProduct},
                    {"new_product", newProduct}};

    json response = putJson("/portfolio/convert-position", payload);
    std::cout << "convertPosition response: " << response.dump(2) << std::endl;
    return response;
  }

  // ─── Market Data ────────────────────────────

  /**
   * Get full market quotes.
   * @param instrumentKeys  Comma-separated instrument keys
   *                        e.g. "NSE_EQ|INE669E01016,NSE_EQ|INE002A01018"
   */
  json getMarketQuote(const std::string &instrumentKeys) {
    return getRequest("/market-quote/quotes?instrument_key=" +
                      urlEncode(instrumentKeys));
  }

  /** Get OHLC data. */
  json getOHLC(const std::string &instrumentKeys,
               const std::string &interval = "1d") {
    return getRequest("/market-quote/ohlc?instrument_key=" +
                      urlEncode(instrumentKeys) + "&interval=" + interval);
  }

  /** Get last traded price. */
  json getLTP(const std::string &instrumentKeys) {
    return getRequest("/market-quote/ltp?instrument_key=" +
                      urlEncode(instrumentKeys));
  }

  /**
   * Get historical candle data.
   *
   * @param instrumentKey  e.g. "NSE_EQ|INE669E01016"
   * @param interval       "1minute", "30minute", "day", "week", "month"
   * @param toDate         End date "YYYY-MM-DD"
   * @param fromDate       Start date "YYYY-MM-DD" (optional for intraday)
   */
  json getHistoricalData(const std::string &instrumentKey,
                         const std::string &interval, const std::string &toDate,
                         const std::string &fromDate = "") {
    std::string url = "/historical-candle/" + urlEncode(instrumentKey) + "/" +
                      interval + "/" + toDate;
    if (!fromDate.empty())
      url += "/" + fromDate;
    return getRequest(url);
  }

  /** Get intraday candle data. */
  json getIntradayData(const std::string &instrumentKey,
                       const std::string &interval) {
    return getRequest("/historical-candle/intraday/" +
                      urlEncode(instrumentKey) + "/" + interval);
  }

  // ─── GTT Orders ─────────────────────────────

  /** Get all GTT orders. */
  json getGttOrders() { return getRequest("/gtt/orders"); }

  /**
   * Place a GTT order.
   */
  json placeGttOrder(const std::string &instrumentToken,
                     const std::string &transactionType,
                     const std::string &product, const std::string &orderType,
                     int quantity, double price, double triggerPrice,
                     const std::string &triggerType = "single") {
    json payload = {{"instrument_token", instrumentToken},
                    {"transaction_type", transactionType},
                    {"product", product},
                    {"order_type", orderType},
                    {"quantity", quantity},
                    {"price", price},
                    {"trigger_price", triggerPrice},
                    {"trigger_type", triggerType}};

    json response = postJson("/gtt/orders", payload);
    std::cout << "placeGttOrder response: " << response.dump(2) << std::endl;
    return response;
  }

  /** Cancel a GTT order. */
  json cancelGttOrder(const std::string &gttOrderId) {
    return deleteRequest("/gtt/orders/" + gttOrderId);
  }

  /** Modify a GTT order. */
  json modifyGttOrder(const std::string &gttOrderId, int quantity, double price,
                      double triggerPrice, const std::string &orderType = "") {
    json payload = {{"quantity", quantity},
                    {"price", price},
                    {"trigger_price", triggerPrice}};
    if (!orderType.empty())
      payload["order_type"] = orderType;

    json response = putJson("/gtt/orders/" + gttOrderId, payload);
    std::cout << "modifyGttOrder response: " << response.dump(2) << std::endl;
    return response;
  }

  // ─── Market Info ────────────────────────────

  /** Get exchange status (market timings). */
  json getExchangeStatus() { return getRequest("/market/status/exchange"); }

  /** Get holidays list. */
  json getHolidays() { return getRequest("/market/holidays"); }

  // ─── Instruments ────────────────────────────

  /**
   * Get instrument master data (returns large CSV, stored as string).
   * Use the instrument_key from response in other API calls.
   */
  json getInstruments(const std::string &exchange = "") {
    std::string url = "/instruments";
    if (!exchange.empty())
      url += "/" + exchange;
    return getRequest(url);
  }

  // ─── Charges ────────────────────────────────

  /** Get brokerage details for an order. */
  json getBrokerage(const std::string &instrumentToken,
                    const std::string &transactionType, int quantity,
                    const std::string &product, const std::string &orderType,
                    double price) {
    std::string url =
        "/charges/brokerage?instrument_token=" + urlEncode(instrumentToken) +
        "&transaction_type=" + transactionType +
        "&quantity=" + std::to_string(quantity) + "&product=" + product +
        "&order_type=" + orderType + "&price=" + std::to_string(price);
    return getRequest(url);
  }

  // ─── Getters / Setters ──────────────────────

  std::string getToken() const { return accessToken_; }
  void setToken(const std::string &token) { accessToken_ = token; }

private:
  std::string accessToken_;
  std::string apiVersion_;
  std::string baseUrl_;

  // ─── Utility ────────────────────────────────

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

  /** Build common headers (Bearer auth + Api-Version). */
  struct curl_slist *buildHeaders(bool isJson = true) {
    struct curl_slist *headers = nullptr;
    if (!accessToken_.empty()) {
      std::string auth = "Authorization: Bearer " + accessToken_;
      headers = curl_slist_append(headers, auth.c_str());
    }
    std::string ver = "Api-Version: " + apiVersion_;
    headers = curl_slist_append(headers, ver.c_str());
    headers = curl_slist_append(headers, "Accept: application/json");
    if (isJson)
      headers = curl_slist_append(headers, "Content-Type: application/json");
    return headers;
  }

  /** Parse Upstox response — extracts "data" from {"status":"success",
   * "data":{...}} */
  json parseResponse(const std::string &responseStr) {
    json response;
    try {
      response = json::parse(responseStr);
    } catch (const json::parse_error &e) {
      std::cerr << "JSON parse error: " << e.what() << std::endl;
      std::cerr << "Raw response: " << responseStr << std::endl;
      throw;
    }

    if (response.contains("status") &&
        response["status"].get<std::string>() == "error") {
      std::string errMsg = "Upstox API error";
      if (response.contains("errors") && response["errors"].is_array() &&
          !response["errors"].empty()) {
        errMsg = response["errors"][0].value("message", errMsg);
      }
      throw std::runtime_error(errMsg);
    }

    if (response.contains("data"))
      return response["data"];
    return response;
  }

  /** GET request. */
  json getRequest(const std::string &path) {
    std::string url = baseUrl_ + path;

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
  json postJson(const std::string &path, const json &payload) {
    std::string url = baseUrl_ + path;
    std::string body = payload.dump();

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

  /** PUT JSON body. */
  json putJson(const std::string &path, const json &payload) {
    std::string url = baseUrl_ + path;
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

  /** DELETE request. */
  json deleteRequest(const std::string &path) {
    std::string url = baseUrl_ + path;

    CURL *curl = curl_easy_init();
    if (!curl)
      throw std::runtime_error("Failed to initialize libcurl");

    std::string responseStr;
    struct curl_slist *headers = buildHeaders(false);

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "DELETE");
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

int main() {
  curl_global_init(CURL_GLOBAL_DEFAULT);

  try {
    // Replace with your actual Upstox Bearer token
    std::string accessToken = "eyJ0eXAiOiJKV1QiLCJrZXlfaWQiOiJza192MS4wIiwiYWxnIjoiSFMyNTYifQ.eyJzdWIiOiI0TUFVUTkiLCJqdGkiOiI2OWEzZDNiMTRmNWJkNzNhYjVmYmNkNjkiLCJpc011bHRpQ2xpZW50IjpmYWxzZSwiaXNQbHVzUGxhbiI6dHJ1ZSwiaWF0IjoxNzcyMzQ0MjQxLCJpc3MiOiJ1ZGFwaS1nYXRld2F5LXNlcnZpY2UiLCJleHAiOjE3NzI0MDI0MDB9.xfa-5GW0lOB9PZYomxAh24pqbPgXD9TYzFswJtHuyN4";

    UpstoxBroker broker(accessToken);

    // ── Example: Get profile ──
    std::cout << "\n=== Profile ===" << std::endl;
    json profile = broker.getProfile();
    std::cout << profile.dump(2) << std::endl;

    // ── Example: Get funds & margin ──
    std::cout << "\n=== Funds & Margin ===" << std::endl;
    json funds = broker.getFundsAndMargin();
    std::cout << funds.dump(2) << std::endl;

    // ── Example: Place a market order ──
    std::cout << "\n=== Place Order ===" << std::endl;
    json orderResp = broker.placeOrder(
        /* instrumentToken  */ "NSE_EQ|INE669E01016",
        /* quantity         */ 1,
        /* product          */ "I",
        /* validity         */ "DAY",
        /* orderType        */ "MARKET",
        /* transactionType  */ "BUY");
    std::cout << orderResp.dump(2) << std::endl;

    // ── Example: Get order book ──
    std::cout << "\n=== Order Book ===" << std::endl;
    json orders = broker.getOrderBook();
    std::cout << orders.dump(2) << std::endl;

    // ── Example: Get positions ──
    std::cout << "\n=== Positions ===" << std::endl;
    json positions = broker.getPositions();
    std::cout << positions.dump(2) << std::endl;

    // ── Example: Get holdings ──
    std::cout << "\n=== Holdings ===" << std::endl;
    json holdings = broker.getHoldings();
    std::cout << holdings.dump(2) << std::endl;

    // ── Example: Get LTP ──
    std::cout << "\n=== LTP ===" << std::endl;
    json ltp = broker.getLTP("NSE_EQ|INE669E01016");
    std::cout << ltp.dump(2) << std::endl;

    // ── Example: Get market quote ──
    std::cout << "\n=== Market Quote ===" << std::endl;
    json quote = broker.getMarketQuote("NSE_EQ|INE669E01016");
    std::cout << quote.dump(2) << std::endl;

  } catch (const std::exception &e) {
    std::cerr << "Error: " << e.what() << std::endl;
    curl_global_cleanup();
    return 1;
  }

  curl_global_cleanup();
  return 0;
}
