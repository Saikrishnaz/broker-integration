/**
 * Zerodha Kite Connect C++ Client
 *
 * Converted from kiteconnect/connect.py — the official Kite Connect API
 * wrapper.
 *
 * Dependencies:
 *   - libcurl       (HTTP requests)
 *   - nlohmann/json (json.hpp, single-header JSON library)
 *   - OpenSSL       (SHA-256 for session token generation)
 *
 * Compile:
 *   g++ -std=c++17 -o kiteconnect kiteconnect.cpp -lcurl -lssl -lcrypto
 *   (MSVC: cl /std:c++17 /EHsc kiteconnect.cpp libcurl.lib libssl.lib
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
//  KiteConnect Class
// ──────────────────────────────────────────────

class KiteConnect {
public:
  // ─── Constants ────────────────────────────

  // Products
  static constexpr const char *PRODUCT_MIS = "MIS";
  static constexpr const char *PRODUCT_CNC = "CNC";
  static constexpr const char *PRODUCT_NRML = "NRML";
  static constexpr const char *PRODUCT_CO = "CO";

  // Order types
  static constexpr const char *ORDER_TYPE_MARKET = "MARKET";
  static constexpr const char *ORDER_TYPE_LIMIT = "LIMIT";
  static constexpr const char *ORDER_TYPE_SLM = "SL-M";
  static constexpr const char *ORDER_TYPE_SL = "SL";

  // Varieties
  static constexpr const char *VARIETY_REGULAR = "regular";
  static constexpr const char *VARIETY_CO = "co";
  static constexpr const char *VARIETY_AMO = "amo";
  static constexpr const char *VARIETY_ICEBERG = "iceberg";
  static constexpr const char *VARIETY_AUCTION = "auction";

  // Transaction types
  static constexpr const char *TRANSACTION_TYPE_BUY = "BUY";
  static constexpr const char *TRANSACTION_TYPE_SELL = "SELL";

  // Validity
  static constexpr const char *VALIDITY_DAY = "DAY";
  static constexpr const char *VALIDITY_IOC = "IOC";
  static constexpr const char *VALIDITY_TTL = "TTL";

  // Position types
  static constexpr const char *POSITION_TYPE_DAY = "day";
  static constexpr const char *POSITION_TYPE_OVERNIGHT = "overnight";

  // Exchanges
  static constexpr const char *EXCHANGE_NSE = "NSE";
  static constexpr const char *EXCHANGE_BSE = "BSE";
  static constexpr const char *EXCHANGE_NFO = "NFO";
  static constexpr const char *EXCHANGE_CDS = "CDS";
  static constexpr const char *EXCHANGE_BFO = "BFO";
  static constexpr const char *EXCHANGE_MCX = "MCX";
  static constexpr const char *EXCHANGE_BCD = "BCD";

  // Margin segments
  static constexpr const char *MARGIN_EQUITY = "equity";
  static constexpr const char *MARGIN_COMMODITY = "commodity";

  // GTT types
  static constexpr const char *GTT_TYPE_OCO = "two-leg";
  static constexpr const char *GTT_TYPE_SINGLE = "single";

  /**
   * Constructor
   *
   * @param apiKey       API key issued by Zerodha
   * @param accessToken  Access token from login flow (can be set later)
   * @param root         API root URL (default: https://api.kite.trade)
   * @param disableSsl   Disable SSL verification
   */
  KiteConnect(const std::string &apiKey, const std::string &accessToken = "",
              const std::string &root = "https://api.kite.trade",
              bool disableSsl = false)
      : apiKey_(apiKey), accessToken_(accessToken), root_(root),
        disableSsl_(disableSsl) {
    std::cout << "KiteConnect initialized for api_key: " << apiKey_
              << std::endl;
  }

  void setAccessToken(const std::string &accessToken) {
    accessToken_ = accessToken;
  }

  std::string getAccessToken() const { return accessToken_; }
  std::string getApiKey() const { return apiKey_; }

  std::string loginUrl() const {
    return "https://kite.zerodha.com/connect/login?api_key=" + apiKey_ + "&v=3";
  }

  // ─── Session / Auth ─────────────────────────

  /**
   * Generate session by exchanging request_token for access_token.
   *
   * @param requestToken  Token obtained from redirect after login
   * @param apiSecret     API secret issued with the API key
   * @return JSON response with access_token, user details, etc.
   */
  json generateSession(const std::string &requestToken,
                       const std::string &apiSecret) {
    std::string checksum = sha256(apiKey_ + requestToken + apiSecret);

    std::map<std::string, std::string> params = {
        {"api_key", apiKey_},
        {"request_token", requestToken},
        {"checksum", checksum}};

    json resp = postForm("/session/token", params);
    if (resp.contains("access_token")) {
      accessToken_ = resp["access_token"].get<std::string>();
    }
    return resp;
  }

  /**
   * Invalidate (logout) an access token.
   */
  json invalidateAccessToken(const std::string &accessToken = "") {
    std::string token = accessToken.empty() ? accessToken_ : accessToken;
    std::map<std::string, std::string> params = {{"api_key", apiKey_},
                                                 {"access_token", token}};
    return deleteRequest("/session/token", params);
  }

  /**
   * Renew access token using refresh token.
   */
  json renewAccessToken(const std::string &refreshToken,
                        const std::string &apiSecret) {
    std::string checksum = sha256(apiKey_ + refreshToken + apiSecret);

    std::map<std::string, std::string> params = {
        {"api_key", apiKey_},
        {"refresh_token", refreshToken},
        {"checksum", checksum}};

    json resp = postForm("/session/refresh_token", params);
    if (resp.contains("access_token")) {
      accessToken_ = resp["access_token"].get<std::string>();
    }
    return resp;
  }

  // ─── User ───────────────────────────────────

  /** Get user profile details. */
  json profile() { return getRequest("/user/profile"); }

  /** Get account margins. Optionally pass segment: "equity" or "commodity". */
  json margins(const std::string &segment = "") {
    if (!segment.empty())
      return getRequest("/user/margins/" + segment);
    return getRequest("/user/margins");
  }

  // ─── Orders ─────────────────────────────────

  /**
   * Place an order.
   *
   * @param variety          "regular", "co", "amo", "iceberg", "auction"
   * @param exchange         "NSE", "BSE", "NFO", "MCX", etc.
   * @param tradingsymbol    Trading symbol e.g. "INFY"
   * @param transactionType  "BUY" or "SELL"
   * @param quantity         Order quantity
   * @param product          "MIS", "CNC", "NRML"
   * @param orderType        "MARKET", "LIMIT", "SL", "SL-M"
   * @param price            Limit price (optional)
   * @param validity         "DAY", "IOC", "TTL" (optional)
   * @param triggerPrice     Trigger price for SL orders (optional)
   * @param disclosedQty     Disclosed quantity (optional)
   * @param tag              Order tag (optional)
   * @return order_id string
   */
  std::string placeOrder(
      const std::string &variety, const std::string &exchange,
      const std::string &tradingsymbol, const std::string &transactionType,
      int quantity, const std::string &product, const std::string &orderType,
      const std::string &price = "", const std::string &validity = "",
      const std::string &triggerPrice = "",
      const std::string &disclosedQty = "", const std::string &tag = "") {
    std::map<std::string, std::string> params = {
        {"exchange", exchange},
        {"tradingsymbol", tradingsymbol},
        {"transaction_type", transactionType},
        {"quantity", std::to_string(quantity)},
        {"product", product},
        {"order_type", orderType}};

    if (!price.empty())
      params["price"] = price;
    if (!validity.empty())
      params["validity"] = validity;
    if (!triggerPrice.empty())
      params["trigger_price"] = triggerPrice;
    if (!disclosedQty.empty())
      params["disclosed_quantity"] = disclosedQty;
    if (!tag.empty())
      params["tag"] = tag;

    json resp = postForm("/orders/" + variety, params);
    std::cout << "placeOrder response: " << resp.dump(2) << std::endl;
    return resp.value("order_id", "");
  }

  /**
   * Modify an open order.
   */
  std::string modifyOrder(
      const std::string &variety, const std::string &orderId,
      const std::string &quantity = "", const std::string &price = "",
      const std::string &orderType = "", const std::string &triggerPrice = "",
      const std::string &validity = "", const std::string &disclosedQty = "",
      const std::string &parentOrderId = "") {
    std::map<std::string, std::string> params;
    if (!quantity.empty())
      params["quantity"] = quantity;
    if (!price.empty())
      params["price"] = price;
    if (!orderType.empty())
      params["order_type"] = orderType;
    if (!triggerPrice.empty())
      params["trigger_price"] = triggerPrice;
    if (!validity.empty())
      params["validity"] = validity;
    if (!disclosedQty.empty())
      params["disclosed_quantity"] = disclosedQty;
    if (!parentOrderId.empty())
      params["parent_order_id"] = parentOrderId;

    json resp = putRequest("/orders/" + variety + "/" + orderId, params);
    std::cout << "modifyOrder response: " << resp.dump(2) << std::endl;
    return resp.value("order_id", "");
  }

  /**
   * Cancel an order.
   */
  std::string cancelOrder(const std::string &variety,
                          const std::string &orderId,
                          const std::string &parentOrderId = "") {
    std::map<std::string, std::string> params;
    if (!parentOrderId.empty())
      params["parent_order_id"] = parentOrderId;

    json resp = deleteRequest("/orders/" + variety + "/" + orderId, params);
    std::cout << "cancelOrder response: " << resp.dump(2) << std::endl;
    return resp.value("order_id", "");
  }

  /** Exit a CO order (alias for cancelOrder). */
  std::string exitOrder(const std::string &variety, const std::string &orderId,
                        const std::string &parentOrderId = "") {
    return cancelOrder(variety, orderId, parentOrderId);
  }

  /** Get list of all orders for the day. */
  json orders() { return getRequest("/orders"); }

  /** Get history of a specific order. */
  json orderHistory(const std::string &orderId) {
    return getRequest("/orders/" + orderId);
  }

  /** Get list of all trades for the day. */
  json trades() { return getRequest("/trades"); }

  /** Get trades for a specific order. */
  json orderTrades(const std::string &orderId) {
    return getRequest("/orders/" + orderId + "/trades");
  }

  // ─── Portfolio ──────────────────────────────

  /** Get positions. */
  json positions() { return getRequest("/portfolio/positions"); }

  /** Get equity holdings. */
  json holdings() { return getRequest("/portfolio/holdings"); }

  /** Get auction instruments. */
  json auctionInstruments() {
    return getRequest("/portfolio/holdings/auctions");
  }

  /**
   * Convert position product type.
   */
  json convertPosition(const std::string &exchange,
                       const std::string &tradingsymbol,
                       const std::string &transactionType,
                       const std::string &positionType, int quantity,
                       const std::string &oldProduct,
                       const std::string &newProduct) {
    std::map<std::string, std::string> params = {
        {"exchange", exchange},
        {"tradingsymbol", tradingsymbol},
        {"transaction_type", transactionType},
        {"position_type", positionType},
        {"quantity", std::to_string(quantity)},
        {"old_product", oldProduct},
        {"new_product", newProduct}};

    return putRequest("/portfolio/positions", params);
  }

  // ─── Market Data ────────────────────────────

  /**
   * Get quote for instruments.
   * Instruments format: "NSE:INFY", "NFO:NIFTY23JUNFUT", etc.
   */
  json quote(const std::vector<std::string> &instruments) {
    std::string queryStr = buildInstrumentQuery(instruments);
    return getRequest("/quote?" + queryStr);
  }

  /** Get OHLC data for instruments. */
  json ohlc(const std::vector<std::string> &instruments) {
    std::string queryStr = buildInstrumentQuery(instruments);
    return getRequest("/quote/ohlc?" + queryStr);
  }

  /** Get last traded price for instruments. */
  json ltp(const std::vector<std::string> &instruments) {
    std::string queryStr = buildInstrumentQuery(instruments);
    return getRequest("/quote/ltp?" + queryStr);
  }

  /**
   * Get historical candle data.
   *
   * @param instrumentToken  Instrument token (numeric)
   * @param interval         "minute", "5minute", "15minute", "day", etc.
   * @param fromDate         Start date "YYYY-MM-DD HH:MM:SS"
   * @param toDate           End date "YYYY-MM-DD HH:MM:SS"
   * @param continuous       Continuous data for futures (0 or 1)
   * @param oi               Include open interest (0 or 1)
   */
  json historicalData(const std::string &instrumentToken,
                      const std::string &interval, const std::string &fromDate,
                      const std::string &toDate, int continuous = 0,
                      int oi = 0) {
    std::string url = "/instruments/historical/" + instrumentToken + "/" +
                      interval + "?from=" + urlEncode(fromDate) +
                      "&to=" + urlEncode(toDate) +
                      "&continuous=" + std::to_string(continuous) +
                      "&oi=" + std::to_string(oi);
    return getRequest(url);
  }

  /**
   * Get trigger range for cover orders.
   */
  json triggerRange(const std::string &transactionType,
                    const std::vector<std::string> &instruments) {
    std::string tt = transactionType;
    std::transform(tt.begin(), tt.end(), tt.begin(), ::tolower);
    std::string queryStr = buildInstrumentQuery(instruments);
    return getRequest("/instruments/trigger_range/" + tt + "?" + queryStr);
  }

  // ─── GTT ────────────────────────────────────

  /** Get all GTT triggers. */
  json getGtts() { return getRequest("/gtt/triggers"); }

  /** Get a specific GTT trigger. */
  json getGtt(const std::string &triggerId) {
    return getRequest("/gtt/triggers/" + triggerId);
  }

  /**
   * Place a GTT order.
   *
   * @param triggerType    "single" or "two-leg"
   * @param tradingsymbol  Trading symbol
   * @param exchange       Exchange
   * @param triggerValues  JSON array of trigger prices
   * @param lastPrice      Last price of the instrument
   * @param orders         JSON array of order objects
   */
  json placeGtt(const std::string &triggerType,
                const std::string &tradingsymbol, const std::string &exchange,
                const json &triggerValues, double lastPrice,
                const json &gttOrders) {
    json condition = {{"exchange", exchange},
                      {"tradingsymbol", tradingsymbol},
                      {"trigger_values", triggerValues},
                      {"last_price", lastPrice}};

    std::map<std::string, std::string> params = {
        {"condition", condition.dump()},
        {"orders", gttOrders.dump()},
        {"type", triggerType}};

    return postForm("/gtt/triggers", params);
  }

  /**
   * Modify a GTT order.
   */
  json modifyGtt(const std::string &triggerId, const std::string &triggerType,
                 const std::string &tradingsymbol, const std::string &exchange,
                 const json &triggerValues, double lastPrice,
                 const json &gttOrders) {
    json condition = {{"exchange", exchange},
                      {"tradingsymbol", tradingsymbol},
                      {"trigger_values", triggerValues},
                      {"last_price", lastPrice}};

    std::map<std::string, std::string> params = {
        {"condition", condition.dump()},
        {"orders", gttOrders.dump()},
        {"type", triggerType}};

    return putRequest("/gtt/triggers/" + triggerId, params);
  }

  /** Delete a GTT trigger. */
  json deleteGtt(const std::string &triggerId) {
    return deleteRequest("/gtt/triggers/" + triggerId);
  }

  // ─── Margins / Charges ──────────────────────

  /**
   * Calculate order margins.
   * @param orderParams  JSON array of order params
   */
  json orderMargins(const json &orderParams) {
    return postJsonBody("/margins/orders", orderParams);
  }

  /**
   * Calculate basket order margins.
   */
  json basketOrderMargins(const json &orderParams,
                          bool considerPositions = true,
                          const std::string &mode = "") {
    std::string url = "/margins/basket?consider_positions=" +
                      std::string(considerPositions ? "true" : "false");
    if (!mode.empty())
      url += "&mode=" + mode;
    return postJsonBody(url, orderParams);
  }

  /**
   * Get virtual contract note / charges.
   */
  json getVirtualContractNote(const json &orderParams) {
    return postJsonBody("/charges/orders", orderParams);
  }

private:
  std::string apiKey_;
  std::string accessToken_;
  std::string root_;
  bool disableSsl_;

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

  /** Build query string like "i=NSE:INFY&i=NFO:NIFTY" */
  static std::string
  buildInstrumentQuery(const std::vector<std::string> &instruments) {
    std::string q;
    for (size_t i = 0; i < instruments.size(); ++i) {
      if (i > 0)
        q += "&";
      q += "i=" + urlEncode(instruments[i]);
    }
    return q;
  }

  /** Build form-encoded body from params map. */
  static std::string
  buildFormBody(const std::map<std::string, std::string> &params) {
    std::string body;
    bool first = true;
    CURL *curl = curl_easy_init();
    for (auto &[key, value] : params) {
      if (!first)
        body += "&";
      first = false;
      char *encodedKey =
          curl_easy_escape(curl, key.c_str(), static_cast<int>(key.length()));
      char *encodedVal = curl_easy_escape(curl, value.c_str(),
                                          static_cast<int>(value.length()));
      body += std::string(encodedKey) + "=" + std::string(encodedVal);
      curl_free(encodedKey);
      curl_free(encodedVal);
    }
    curl_easy_cleanup(curl);
    return body;
  }

  // ─── HTTP Helpers ───────────────────────────

  /** Build authorization header list. */
  struct curl_slist *buildHeaders(bool isJson = false) {
    struct curl_slist *headers = nullptr;

    // Kite uses "token api_key:access_token" format
    if (!apiKey_.empty() && !accessToken_.empty()) {
      std::string auth = "Authorization: token " + apiKey_ + ":" + accessToken_;
      headers = curl_slist_append(headers, auth.c_str());
    }

    headers = curl_slist_append(headers, "X-Kite-Version: 3");

    if (isJson) {
      headers = curl_slist_append(headers, "Content-Type: application/json");
    } else {
      headers = curl_slist_append(
          headers, "Content-Type: application/x-www-form-urlencoded");
    }

    return headers;
  }

  /**
   * Parse Kite API response — extracts "data" from {"status":"success",
   * "data":{...}}
   */
  json parseResponse(const std::string &responseStr) {
    json response;
    try {
      response = json::parse(responseStr);
    } catch (const json::parse_error &e) {
      std::cerr << "JSON parse error: " << e.what() << std::endl;
      std::cerr << "Raw response: " << responseStr << std::endl;
      throw;
    }

    // Check for API errors
    if (response.contains("status") &&
        response["status"].get<std::string>() == "error") {
      std::string errMsg = response.value("message", "Unknown Kite API error");
      std::string errType = response.value("error_type", "GeneralException");
      throw std::runtime_error("[" + errType + "] " + errMsg);
    }

    // Return the "data" field
    if (response.contains("data"))
      return response["data"];
    return response;
  }

  /** GET request. */
  json getRequest(const std::string &path,
                  const std::map<std::string, std::string> &queryParams = {}) {
    std::string url = root_ + path;

    // Append query params if any
    if (!queryParams.empty()) {
      url += (path.find('?') != std::string::npos ? "&" : "?");
      url += buildFormBody(queryParams);
    }

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
    if (disableSsl_) {
      curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
      curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
    }

    CURLcode res = curl_easy_perform(curl);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK)
      throw std::runtime_error(std::string("HTTP GET failed: ") +
                               curl_easy_strerror(res));

    return parseResponse(responseStr);
  }

  /** POST with form-encoded body. */
  json postForm(const std::string &path,
                const std::map<std::string, std::string> &params) {
    std::string url = root_ + path;
    std::string body = buildFormBody(params);

    CURL *curl = curl_easy_init();
    if (!curl)
      throw std::runtime_error("Failed to initialize libcurl");

    std::string responseStr;
    struct curl_slist *headers = buildHeaders(false);

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &responseStr);
    if (disableSsl_) {
      curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
      curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
    }

    CURLcode res = curl_easy_perform(curl);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK)
      throw std::runtime_error(std::string("HTTP POST failed: ") +
                               curl_easy_strerror(res));

    return parseResponse(responseStr);
  }

  /** POST with JSON body (for margin calculations). */
  json postJsonBody(const std::string &path, const json &payload) {
    std::string url = root_ + path;
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
    if (disableSsl_) {
      curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
      curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
    }

    CURLcode res = curl_easy_perform(curl);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK)
      throw std::runtime_error(std::string("HTTP POST (JSON) failed: ") +
                               curl_easy_strerror(res));

    return parseResponse(responseStr);
  }

  /** PUT request with form-encoded body. */
  json putRequest(const std::string &path,
                  const std::map<std::string, std::string> &params) {
    std::string url = root_ + path;
    std::string body = buildFormBody(params);

    CURL *curl = curl_easy_init();
    if (!curl)
      throw std::runtime_error("Failed to initialize libcurl");

    std::string responseStr;
    struct curl_slist *headers = buildHeaders(false);

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PUT");
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &responseStr);
    if (disableSsl_) {
      curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
      curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
    }

    CURLcode res = curl_easy_perform(curl);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK)
      throw std::runtime_error(std::string("HTTP PUT failed: ") +
                               curl_easy_strerror(res));

    return parseResponse(responseStr);
  }

  /** DELETE request. */
  json deleteRequest(const std::string &path,
                     const std::map<std::string, std::string> &params = {}) {
    std::string url = root_ + path;

    // DELETE sends params as query string
    if (!params.empty()) {
      url += "?" + buildFormBody(params);
    }

    CURL *curl = curl_easy_init();
    if (!curl)
      throw std::runtime_error("Failed to initialize libcurl");

    std::string responseStr;
    struct curl_slist *headers = buildHeaders();

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "DELETE");
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &responseStr);
    if (disableSsl_) {
      curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
      curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
    }

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
//     // Replace with your actual API key and access token
//     std::string apiKey = "h63xizkk69d4im7w";
//     std::string accessToken = "NDcV1XpOLXm4A986vbBbMAh4E5k6g1Mw";

//     KiteConnect kite(apiKey, accessToken);

//     // ── Example: Get profile ──
//     std::cout << "\n=== Profile ===" << std::endl;
//     json prof = kite.profile();
//     std::cout << prof.dump(2) << std::endl;

//     // ── Example: Get margins ──
//     std::cout << "\n=== Margins ===" << std::endl;
//     json marg = kite.margins();
//     std::cout << marg.dump(2) << std::endl;

//     // ── Example: Place a regular market order ──
//     std::cout << "\n=== Place Order ===" << std::endl;
//     std::string orderId = kite.placeOrder(
//         /* variety          */ KiteConnect::VARIETY_REGULAR,
//         /* exchange         */ KiteConnect::EXCHANGE_NSE,
//         /* tradingsymbol    */ "INFY",
//         /* transactionType  */ KiteConnect::TRANSACTION_TYPE_BUY,
//         /* quantity         */ 1,
//         /* product          */ KiteConnect::PRODUCT_CNC,
//         /* orderType        */ KiteConnect::ORDER_TYPE_MARKET);
//     std::cout << "Order ID: " << orderId << std::endl;

//     // ── Example: Get order book ──
//     std::cout << "\n=== Orders ===" << std::endl;
//     json allOrders = kite.orders();
//     std::cout << allOrders.dump(2) << std::endl;

//     // ── Example: Get positions ──
//     std::cout << "\n=== Positions ===" << std::endl;
//     json pos = kite.positions();
//     std::cout << pos.dump(2) << std::endl;

//     // ── Example: Get holdings ──
//     std::cout << "\n=== Holdings ===" << std::endl;
//     json hold = kite.holdings();
//     std::cout << hold.dump(2) << std::endl;

//     // ── Example: Get LTP ──
//     std::cout << "\n=== LTP ===" << std::endl;
//     json ltpData = kite.ltp({"NSE:INFY", "NSE:RELIANCE"});
//     std::cout << ltpData.dump(2) << std::endl;

//     // ── Example: Get GTT triggers ──
//     std::cout << "\n=== GTT Triggers ===" << std::endl;
//     json gtts = kite.getGtts();
//     std::cout << gtts.dump(2) << std::endl;

//   } catch (const std::exception &e) {
//     std::cerr << "Error: " << e.what() << std::endl;
//     curl_global_cleanup();
//     return 1;
//   }

//   curl_global_cleanup();
//   return 0;
// }
