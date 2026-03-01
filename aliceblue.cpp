/**
 * AliceBlue Broker C++ Client
 *
 * Based on the AliceBlue A3 Open API (Postman collection reference).
 * Uses Bearer token authentication — token must be obtained externally
 * (via OAuth/checkSum flow) and passed to the constructor.
 *
 * Dependencies:
 *   - libcurl       (HTTP requests)
 *   - nlohmann/json (json.hpp, single-header JSON library)
 *
 * Compile:
 *   g++ -std=c++17 -o aliceblue aliceblue.cpp -lcurl
 *   (On Windows with MSVC: cl /std:c++17 /EHsc aliceblue.cpp libcurl.lib)
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
//  AliceBlueBroker Class
// ──────────────────────────────────────────────

class AliceBlueBroker {
public:
  /**
   * Constructor
   *
   * @param token  Bearer token obtained from AliceBlue OAuth flow
   *               (getUserDetails checkSum → session token)
   */
  explicit AliceBlueBroker(const std::string &token) : token_(token) {
    baseUrl_ = "https://a3.aliceblueonline.com/open-api/od/v1";
    optionChainBaseUrl_ = "https://a3.aliceblueonline.com/obrest/optionChain";

    // Build route map
    placeOrderRoute_ = baseUrl_ + "/orders/placeorder";
    modifyOrderRoute_ = baseUrl_ + "/orders/modify";
    cancelOrderRoute_ = baseUrl_ + "/orders/cancel";
    orderBookRoute_ = baseUrl_ + "/orders/book";
    orderHistoryRoute_ = baseUrl_ + "/orders/history";
    orderTradesRoute_ = baseUrl_ + "/orders/trades";
    checkMarginRoute_ = baseUrl_ + "/orders/checkMargin";
    exitBracketRoute_ = baseUrl_ + "/orders/exit/sno";
    basketMarginRoute_ = baseUrl_ + "/orders/basket/margin";

    // GTT
    gttOrderbookRoute_ = baseUrl_ + "/orders/gtt/orderbook";
    gttCancelRoute_ = baseUrl_ + "/orders/gtt/cancel";
    gttExecuteRoute_ = baseUrl_ + "/orders/gtt/execute";
    gttModifyRoute_ = baseUrl_ + "/orders/gtt/modify";

    // Positions
    positionsRoute_ = baseUrl_ + "/positions";
    squareOffRoute_ = baseUrl_ + "/orders/positions/sqroff";
    conversionRoute_ = baseUrl_ + "/conversion";

    // Holdings, Limits, Profile
    holdingsRoute_ = baseUrl_ + "/holdings";
    limitsRoute_ = baseUrl_ + "/limits/";
    profileRoute_ = baseUrl_ + "/profile/";
    getUserDetailsRoute_ = baseUrl_ + "/vendor/getUserDetails";

    // Option Chain
    getUnderlyingRoute_ = optionChainBaseUrl_ + "/getUnderlying";
    getUnderlyingExpRoute_ = optionChainBaseUrl_ + "/getUnderlyingExp";
    getOptionChainRoute_ = optionChainBaseUrl_ + "/getOptionChain";

    std::cout << "AliceBlue Broker initialized." << std::endl;
  }

  // ─── Auth ───────────────────────────────────

  /**
   * Get user session details using checkSum.
   * This is typically the first call to validate credentials.
   */
  json getUserDetails(const std::string &checkSum) {
    json payload = {{"checkSum", checkSum}};
    return postJson(getUserDetailsRoute_, payload, false);
  }

  // ─── Orders ─────────────────────────────────

  /**
   * Place an order.
   *
   * @param exchange        Exchange: "NSE", "BSE", "NFO", "MCX", "BFO"
   * @param instrumentId    Instrument ID (numeric scrip code as string)
   * @param transactionType "BUY" or "SELL"
   * @param quantity        Order quantity
   * @param product         "INTRADAY", "LONGTERM", "CNC", "MTF"
   * @param orderComplexity "REGULAR", "BO" (Bracket Order)
   * @param orderType       "MARKET", "LIMIT", "SL", "SL-M"
   * @param validity        "DAY", "IOC"
   * @param price           Limit price (use "0" for market orders)
   * @param slTriggerPrice  Stop-loss trigger price (optional)
   * @param slLegPrice      Stop-loss leg price for BO (optional)
   * @param targetLegPrice  Target leg price for BO (optional)
   * @param disclosedQty    Disclosed quantity (optional)
   * @param marketProtPct   Market protection percent (optional)
   * @param trailingSlAmt   Trailing SL amount (optional)
   * @param orderTag        Custom order tag (optional)
   * @param algoId          Algo ID (optional)
   * @param apiOrderSource  API order source (optional)
   */
  json placeOrder(
      const std::string &exchange, const std::string &instrumentId,
      const std::string &transactionType, int quantity,
      const std::string &product, const std::string &orderComplexity,
      const std::string &orderType, const std::string &validity,
      const std::string &price = "0", const std::string &slTriggerPrice = "",
      const std::string &slLegPrice = "",
      const std::string &targetLegPrice = "",
      const std::string &disclosedQty = "",
      const std::string &marketProtPct = "",
      const std::string &trailingSlAmt = "", const std::string &orderTag = "",
      const std::string &algoId = "", const std::string &apiOrderSource = "") {
    // AliceBlue expects an array of order objects
    json orderObj = {{"exchange", exchange},
                     {"instrumentId", instrumentId},
                     {"transactionType", transactionType},
                     {"quantity", std::to_string(quantity)},
                     {"product", product},
                     {"orderComplexity", orderComplexity},
                     {"orderType", orderType},
                     {"validity", validity},
                     {"price", price},
                     {"slTriggerPrice", slTriggerPrice},
                     {"slLegPrice", slLegPrice},
                     {"targetLegPrice", targetLegPrice},
                     {"disclosedQuantity", disclosedQty},
                     {"marketProtectionPercent", marketProtPct},
                     {"trailingSlAmount", trailingSlAmt},
                     {"apiOrderSource", apiOrderSource},
                     {"algoId", algoId},
                     {"orderTag", orderTag}};

    json payload = json::array({orderObj});

    json response = postJsonRaw(placeOrderRoute_, payload.dump());
    std::cout << "placeOrder response: " << response.dump(2) << std::endl;
    return response;
  }

  /**
   * Modify an existing order.
   */
  json modifyOrder(const std::string &brokerOrderId, int quantity,
                   const std::string &orderType, const std::string &validity,
                   const std::string &price = "",
                   const std::string &slTriggerPrice = "",
                   const std::string &slLegPrice = "",
                   const std::string &targetLegPrice = "",
                   const std::string &trailingSLAmount = "",
                   const std::string &disclosedQty = "0",
                   const std::string &marketProtection = "") {
    json payload = {{"brokerOrderId", brokerOrderId},
                    {"quantity", quantity},
                    {"orderType", orderType},
                    {"validity", validity},
                    {"price", price},
                    {"slTriggerPrice", slTriggerPrice},
                    {"slLegPrice", slLegPrice},
                    {"targetLegPrice", targetLegPrice},
                    {"trailingSLAmount", trailingSLAmount},
                    {"disclosedQuantity", disclosedQty},
                    {"marketProtection", marketProtection}};

    json response = postJson(modifyOrderRoute_, payload);
    std::cout << "modifyOrder response: " << response.dump(2) << std::endl;
    return response;
  }

  /**
   * Cancel an order.
   */
  json cancelOrder(const std::string &brokerOrderId) {
    json payload = {{"brokerOrderId", brokerOrderId}};
    json response = postJson(cancelOrderRoute_, payload);
    std::cout << "cancelOrder response: " << response.dump(2) << std::endl;
    return response;
  }

  /**
   * Get order book (all orders for the day).
   */
  json getOrderBook() {
    json response = getJson(orderBookRoute_);
    std::cout << "orderBook response: " << response.dump(2) << std::endl;
    return response;
  }

  /**
   * Get history for a specific order.
   */
  json getOrderHistory(const std::string &brokerOrderId) {
    json payload = {{"brokerOrderId", brokerOrderId}};
    json response = postJson(orderHistoryRoute_, payload);
    std::cout << "orderHistory response: " << response.dump(2) << std::endl;
    return response;
  }

  /**
   * Get all trades for the day.
   */
  json getTrades() {
    json response = getJson(orderTradesRoute_);
    std::cout << "trades response: " << response.dump(2) << std::endl;
    return response;
  }

  /**
   * Check margin for an order before placing it.
   */
  json checkMargin(const std::string &exchange, const std::string &instrumentId,
                   const std::string &transactionType, int quantity,
                   const std::string &product,
                   const std::string &orderComplexity,
                   const std::string &orderType, double price,
                   const std::string &validity = "DAY",
                   const std::string &slLegPrice = "",
                   const std::string &slTriggerPrice = "0",
                   const std::string &targetLegPrice = "") {
    json payload = {{"exchange", exchange},
                    {"instrumentId", instrumentId},
                    {"transactionType", transactionType},
                    {"quantity", quantity},
                    {"product", product},
                    {"orderComplexity", orderComplexity},
                    {"orderType", orderType},
                    {"price", price},
                    {"validity", validity},
                    {"slLegPrice", slLegPrice},
                    {"slTriggerPrice", slTriggerPrice},
                    {"targetLegPrice", targetLegPrice}};

    json response = postJson(checkMarginRoute_, payload);
    std::cout << "checkMargin response: " << response.dump(2) << std::endl;
    return response;
  }

  /**
   * Exit a bracket order (BO/SNO).
   */
  json exitBracketOrder(const std::string &brokerOrderId,
                        const std::string &orderComplexity = "BO") {
    json orderObj = {{"brokerOrderId", brokerOrderId},
                     {"orderComplexity", orderComplexity}};
    json payload = json::array({orderObj});

    json response = postJsonRaw(exitBracketRoute_, payload.dump());
    std::cout << "exitBracketOrder response: " << response.dump(2) << std::endl;
    return response;
  }

  /**
   * Get basket margin for a list of orders.
   */
  json getBasketMargin(const json &basketOrders) {
    json response = postJsonRaw(basketMarginRoute_, basketOrders.dump());
    std::cout << "basketMargin response: " << response.dump(2) << std::endl;
    return response;
  }

  // ─── GTT Orders ─────────────────────────────

  /**
   * Get GTT order book.
   */
  json getGttOrderbook() {
    json response = getJson(gttOrderbookRoute_);
    std::cout << "gttOrderbook response: " << response.dump(2) << std::endl;
    return response;
  }

  /**
   * Cancel a GTT order.
   */
  json cancelGttOrder(const std::string &brokerOrderId) {
    json payload = {{"brokerOrderId", brokerOrderId}};
    json response = postJson(gttCancelRoute_, payload);
    std::cout << "gttCancel response: " << response.dump(2) << std::endl;
    return response;
  }

  /**
   * Place a GTT order.
   */
  json placeGttOrder(const std::string &tradingSymbol,
                     const std::string &exchange,
                     const std::string &transactionType,
                     const std::string &orderType, const std::string &product,
                     const std::string &validity, const std::string &quantity,
                     double price, const std::string &orderComplexity,
                     const std::string &instrumentId,
                     const std::string &gttValue) {
    json payload = {{"tradingSymbol", tradingSymbol},
                    {"exchange", exchange},
                    {"transactionType", transactionType},
                    {"orderType", orderType},
                    {"product", product},
                    {"validity", validity},
                    {"quantity", quantity},
                    {"price", price},
                    {"orderComplexity", orderComplexity},
                    {"instrumentId", instrumentId},
                    {"gttValue", gttValue}};

    json response = postJson(gttExecuteRoute_, payload);
    std::cout << "gttPlaceOrder response: " << response.dump(2) << std::endl;
    return response;
  }

  /**
   * Modify a GTT order.
   */
  json modifyGttOrder(const std::string &brokerOrderId,
                      const std::string &tradingSymbol,
                      const std::string &exchange, const std::string &orderType,
                      const std::string &product, const std::string &validity,
                      const std::string &quantity, const std::string &price,
                      const std::string &orderComplexity,
                      const std::string &instrumentId,
                      const std::string &gttValue) {
    json payload = {{"brokerOrderId", brokerOrderId},
                    {"tradingSymbol", tradingSymbol},
                    {"exchange", exchange},
                    {"orderType", orderType},
                    {"product", product},
                    {"validity", validity},
                    {"quantity", quantity},
                    {"price", price},
                    {"orderComplexity", orderComplexity},
                    {"instrumentId", instrumentId},
                    {"gttValue", gttValue}};

    json response = postJson(gttModifyRoute_, payload);
    std::cout << "gttModifyOrder response: " << response.dump(2) << std::endl;
    return response;
  }

  // ─── Positions ──────────────────────────────

  /**
   * Get all positions.
   */
  json getPositions() {
    json response = getJson(positionsRoute_);
    std::cout << "positions response: " << response.dump(2) << std::endl;
    return response;
  }

  /**
   * Square off a position.
   */
  json squareOffPosition(
      const std::string &instrumentId, const std::string &exchange,
      const std::string &transactionType, int quantity,
      const std::string &orderComplexity, const std::string &product,
      const std::string &orderType, const std::string &price,
      const std::string &validity = "DAY",
      const std::string &slTriggerPrice = "",
      const std::string &marketProtPct = "",
      const std::string &targetLegPrice = "",
      const std::string &slLegPrice = "", const std::string &trailingSlAmt = "",
      const std::string &apiOrderSource = "WEB", const std::string &algoId = "",
      const std::string &disclosedQty = "", const std::string &orderTag = "") {
    json orderObj = {{"instrumentId", instrumentId},
                     {"exchange", exchange},
                     {"transactionType", transactionType},
                     {"quantity", quantity},
                     {"orderComplexity", orderComplexity},
                     {"product", product},
                     {"orderType", orderType},
                     {"price", price},
                     {"validity", validity},
                     {"slTriggerPrice", slTriggerPrice},
                     {"marketProtectionPercent", marketProtPct},
                     {"targetLegPrice", targetLegPrice},
                     {"slLegPrice", slLegPrice},
                     {"trailingSlAmount", trailingSlAmt},
                     {"apiOrderSource", apiOrderSource},
                     {"algoId", algoId},
                     {"disclosedQuantity", disclosedQty},
                     {"orderTag", orderTag}};

    json payload = json::array({orderObj});
    json response = postJsonRaw(squareOffRoute_, payload.dump());
    std::cout << "squareOff response: " << response.dump(2) << std::endl;
    return response;
  }

  /**
   * Convert position product type (e.g. MIS → CNC).
   */
  json convertPosition(const std::string &exchange, const std::string &validity,
                       const std::string &prevProduct,
                       const std::string &product, const std::string &quantity,
                       const std::string &tradingSymbol,
                       const std::string &transactionType,
                       const std::string &orderSource = "MOB") {
    json payload = {{"exchange", exchange},
                    {"validity", validity},
                    {"prevProduct", prevProduct},
                    {"product", product},
                    {"quantity", quantity},
                    {"tradingSymbol", tradingSymbol},
                    {"transactionType", transactionType},
                    {"orderSource", orderSource}};

    json response = postJson(conversionRoute_, payload);
    std::cout << "convertPosition response: " << response.dump(2) << std::endl;
    return response;
  }

  // ─── Holdings ───────────────────────────────

  /**
   * Get holdings.
   *
   * @param holdingType  "CNC" (default), "MTF", etc.
   */
  json getHoldings(const std::string &holdingType = "CNC") {
    std::string url = holdingsRoute_ + "/" + holdingType;
    json response = getJson(url);
    std::cout << "holdings response: " << response.dump(2) << std::endl;
    return response;
  }

  // ─── Limits ─────────────────────────────────

  /**
   * Get account limits / funds.
   */
  json getLimits() {
    json response = getJson(limitsRoute_);
    std::cout << "limits response: " << response.dump(2) << std::endl;
    return response;
  }

  // ─── Profile ────────────────────────────────

  /**
   * Get user profile.
   */
  json getProfile() {
    json response = getJson(profileRoute_);
    std::cout << "profile response: " << response.dump(2) << std::endl;
    return response;
  }

  // ─── Option Chain ───────────────────────────

  /**
   * Get underlying instruments for option chain.
   *
   * @param exch  Exchange segment: "nse_fo", "bse_fo", "mcx_fo"
   */
  json getUnderlying(const std::string &exch) {
    json payload = {{"exch", exch}};
    json response = postJson(getUnderlyingRoute_, payload);
    std::cout << "getUnderlying response: " << response.dump(2) << std::endl;
    return response;
  }

  /**
   * Get expiry dates for an underlying.
   */
  json getUnderlyingExpiry(const std::string &underlying,
                           const std::string &exch) {
    json payload = {{"underlying", underlying}, {"exch", exch}};
    json response = postJson(getUnderlyingExpRoute_, payload);
    std::cout << "getUnderlyingExpiry response: " << response.dump(2)
              << std::endl;
    return response;
  }

  /**
   * Get full option chain.
   *
   * @param underlying  e.g. "NIFTY"
   * @param expiry      e.g. "25NOV25"
   * @param interval    Strike interval (e.g. 10)
   * @param exch        Exchange segment
   */
  json getOptionChain(const std::string &underlying, const std::string &expiry,
                      int interval, const std::string &exch) {
    json payload = {{"underlying", underlying},
                    {"expiry", expiry},
                    {"interval", interval},
                    {"exch", exch}};
    json response = postJson(getOptionChainRoute_, payload);
    std::cout << "optionChain response: " << response.dump(2) << std::endl;
    return response;
  }

  // ─── Getters ────────────────────────────────

  std::string getToken() const { return token_; }

  void setToken(const std::string &token) { token_ = token; }

private:
  std::string token_;

  // Base URLs
  std::string baseUrl_;
  std::string optionChainBaseUrl_;

  // Route URLs
  std::string placeOrderRoute_;
  std::string modifyOrderRoute_;
  std::string cancelOrderRoute_;
  std::string orderBookRoute_;
  std::string orderHistoryRoute_;
  std::string orderTradesRoute_;
  std::string checkMarginRoute_;
  std::string exitBracketRoute_;
  std::string basketMarginRoute_;

  std::string gttOrderbookRoute_;
  std::string gttCancelRoute_;
  std::string gttExecuteRoute_;
  std::string gttModifyRoute_;

  std::string positionsRoute_;
  std::string squareOffRoute_;
  std::string conversionRoute_;

  std::string holdingsRoute_;
  std::string limitsRoute_;
  std::string profileRoute_;
  std::string getUserDetailsRoute_;

  std::string getUnderlyingRoute_;
  std::string getUnderlyingExpRoute_;
  std::string getOptionChainRoute_;

  // ─── HTTP Helpers ───────────────────────────

  /**
   * Build curl header list with Bearer auth.
   */
  struct curl_slist *buildHeaders(bool includeAuth = true) {
    struct curl_slist *headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    if (includeAuth && !token_.empty()) {
      std::string authHeader = "Authorization: Bearer " + token_;
      headers = curl_slist_append(headers, authHeader.c_str());
    }
    return headers;
  }

  /**
   * POST JSON object to a URL.
   */
  json postJson(const std::string &url, const json &payload,
                bool includeAuth = true) {
    return postJsonRaw(url, payload.dump(), includeAuth);
  }

  /**
   * POST raw JSON string to a URL (used when payload is a JSON array).
   */
  json postJsonRaw(const std::string &url, const std::string &body,
                   bool includeAuth = true) {
    CURL *curl = curl_easy_init();
    if (!curl)
      throw std::runtime_error("Failed to initialize libcurl");

    std::string responseStr;
    struct curl_slist *headers = buildHeaders(includeAuth);

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

    if (res != CURLE_OK) {
      throw std::runtime_error(std::string("HTTP POST failed: ") +
                               curl_easy_strerror(res));
    }

    try {
      return json::parse(responseStr);
    } catch (const json::parse_error &e) {
      std::cerr << "JSON parse error: " << e.what() << std::endl;
      std::cerr << "Raw response: " << responseStr << std::endl;
      throw;
    }
  }

  /**
   * GET JSON from a URL.
   */
  json getJson(const std::string &url) {
    CURL *curl = curl_easy_init();
    if (!curl)
      throw std::runtime_error("Failed to initialize libcurl");

    std::string responseStr;
    struct curl_slist *headers = buildHeaders(true);

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

    if (res != CURLE_OK) {
      throw std::runtime_error(std::string("HTTP GET failed: ") +
                               curl_easy_strerror(res));
    }

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
//  main() – Usage demo (mirrors td_order.cpp style)
// ──────────────────────────────────────────────

// int main() {
//   curl_global_init(CURL_GLOBAL_DEFAULT);

//   try {
//     // Replace with your actual AliceBlue Bearer token
//     std::string token = "eyJhbGciOiJSUzI1NiIsInR5cCIgOiAiSldUIiwia2lkIiA6ICIyam9lOFVScGxZU3FTcDB3RDNVemVBQkgxYkpmOE4wSDRDMGVVSWhXUVAwIn0.eyJleHAiOjE3Nzc0NzEzODYsImlhdCI6MTc3MjI4OTU0NywianRpIjoib25ydHJ0OjRmNDU3OGNjLTk3NWEtOWU0ZC0zMDJlLTdmZjE5ZWFkZTA4NCIsImlzcyI6Imh0dHBzOi8vaWRhYXMuYWxpY2VibHVlb25saW5lLmNvbS9pZGFhcy9yZWFsbXMvQWxpY2VCbHVlIiwiYXVkIjoiYWNjb3VudCIsInN1YiI6IjVlZjY2YmZiLTRjNWQtNDM5OS1iNmM2LTdjODViNjE0NjU2ZiIsInR5cCI6IkJlYXJlciIsImF6cCI6ImFsaWNlLWtiIiwic2lkIjoiYzlkMjdhNjQtNThkMi0yMzg3LTFhZmUtM2ZjYTgyZDE5YTNmIiwiYWxsb3dlZC1vcmlnaW5zIjpbImh0dHA6Ly9sb2NhbGhvc3Q6MzAwMiIsImh0dHA6Ly9sb2NhbGhvc3Q6NTA1MCIsImh0dHA6Ly9sb2NhbGhvc3Q6OTk0MyIsImh0dHA6Ly9sb2NhbGhvc3Q6OTAwMCJdLCJyZWFsbV9hY2Nlc3MiOnsicm9sZXMiOlsib2ZmbGluZV9hY2Nlc3MiLCJkZWZhdWx0LXJvbGVzLWFsaWNlYmx1ZWtiIiwidW1hX2F1dGhvcml6YXRpb24iXX0sInJlc291cmNlX2FjY2VzcyI6eyJhbGljZS1rYiI6eyJyb2xlcyI6WyJHVUVTVF9VU0VSIiwiQUNUSVZFX1VTRVIiXX0sImFjY291bnQiOnsicm9sZXMiOlsibWFuYWdlLWFjY291bnQiLCJtYW5hZ2UtYWNjb3VudC1saW5rcyIsInZpZXctcHJvZmlsZSJdfX0sInNjb3BlIjoiZW1haWwgcHJvZmlsZSBvcGVuaWQiLCJlbWFpbF92ZXJpZmllZCI6dHJ1ZSwidWNjIjoiMTgwNTY1NiIsImNsaWVudFJvbGUiOlsiR1VFU1RfVVNFUiIsIkFDVElWRV9VU0VSIl0sIm5hbWUiOiJKIFNhaSBLcmlzaG5hIiwicHJlZmVycmVkX3VzZXJuYW1lIjoiMTgwNTY1NiIsImdpdmVuX25hbWUiOiJKIFNhaSBLcmlzaG5hIn0.YYha4jvOKaDVxckOaBXz8TgKTM7p53ntVz1katzNKd9puMgcR745-f9-5Ol3biN_U-ig0nrFnmzgqASGUU_3pB3jK7DsNxrlecLb5wbdbF7F_Bk9pBCXrUdkAYAAXurhttacPyjADdbIbMGkrUv9q_RtqCU3mE-ICyZx7MnxMksOxAyexyMcAzO4MCJoCKXtPHeuE_fqMpR0Xc8_l2ksM5xFYXtQMviaYJT3runBNLUziLaoDuy6654XDAkfAw84unaL_A1nWXSjCdCs6V5HhrB9iJl5tMfq7C2EcmFqppUTBN-JNwy55V8RHc7kTCq3fcDIMOFZRESRGjgy7D_puA";

//     AliceBlueBroker broker(token);

//     // ── Example: Place a market order on NFO ──
//     // json orderResponse = broker.placeOrder(
//     //     /* exchange        */ "NFO",
//     //     /* instrumentId    */ "35229",
//     //     /* transactionType */ "BUY",
//     //     /* quantity        */ 65,
//     //     /* product         */ "INTRADAY",
//     //     /* orderComplexity */ "REGULAR",
//     //     /* orderType       */ "MARKET",
//     //     /* validity        */ "DAY");

//     // std::cout << "\n=== Place Order Response ===" << std::endl;
//     // std::cout << orderResponse.dump(2) << std::endl;
//     json Orderhistory = broker.getOrderBook();
//     std::cout<<"order history : " << Orderhistory<<std::endl;
//     // // ── Example: Get positions ──
//     // std::cout << "\n=== Positions ===" << std::endl;
//     // json positions = broker.getPositions();
//     // std::cout << positions.dump(2) << std::endl;

//     // // ── Example: Get order book ──
//     // std::cout << "\n=== Order Book ===" << std::endl;
//     // json orderBook = broker.getOrderBook();
//     // std::cout << orderBook.dump(2) << std::endl;

//     // // ── Example: Get limits ──
//     // std::cout << "\n=== Limits ===" << std::endl;
//     // json limits = broker.getLimits();
//     // std::cout << limits.dump(2) << std::endl;

//     // // ── Example: Get holdings ──
//     // std::cout << "\n=== Holdings ===" << std::endl;
//     // json holdings = broker.getHoldings("CNC");
//     // std::cout << holdings.dump(2) << std::endl;

//     // // ── Example: Get profile ──
//     // std::cout << "\n=== Profile ===" << std::endl;
//     // json profile = broker.getProfile();
//     // std::cout << profile.dump(2) << std::endl;

//   } catch (const std::exception &e) {
//     std::cerr << "Error: " << e.what() << std::endl;
//     curl_global_cleanup();
//     return 1;
//   }

//   curl_global_cleanup();
//   return 0;
// }
