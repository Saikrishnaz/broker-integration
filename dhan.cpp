/**
 * Dhan (DhanHQ) C++ Client
 *
 * Converted from the dhanhq Python package — provides a class for
 * interacting with the Dhan broker API v2.
 *
 * Dependencies:
 *   - libcurl       (HTTP requests)
 *   - nlohmann/json (JSON library)
 *
 * Compile:
 *   g++ -std=c++17 -o dhan dhan.cpp -lcurl
 *   (MSVC: cl /std:c++17 /EHsc dhan.cpp libcurl.lib)
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
//  DhanBroker Class
// ──────────────────────────────────────────────

class DhanBroker {
public:
  /**
   * Constructor
   *
   * @param clientId     Dhan client ID
   * @param accessToken  Dhan access token
   */
  DhanBroker(const std::string &clientId, const std::string &accessToken)
      : clientId_(clientId), accessToken_(accessToken) {
    baseUrl_ = "https://api.dhan.co/v2";
    std::cout << "Dhan Broker initialized for client: " << clientId_
              << std::endl;
  }

  // ─── Orders ─────────────────────────────────

  /**
   * Place a new order.
   *
   * @param securityId        Security ID to trade
   * @param exchangeSegment   "NSE_EQ", "BSE_EQ", "NSE_FNO", "BSE_FNO",
   *                          "MCX_COMM", "NSE_CURRENCY"
   * @param transactionType   "BUY" or "SELL"
   * @param quantity          Order quantity
   * @param orderType         "LIMIT", "MARKET", "STOP_LOSS", "STOP_LOSS_MARKET"
   * @param productType       "CNC", "INTRA", "MARGIN", "CO", "BO", "MTF"
   * @param price             Order price (0 for MARKET)
   * @param triggerPrice      Trigger price for SL orders
   * @param disclosedQty      Disclosed quantity
   * @param afterMarketOrder  Is AMO order
   * @param validity          "DAY" or "IOC"
   * @param amoTime           "OPEN", "OPEN_30", "OPEN_60"
   * @param boProfitValue     BO take-profit value
   * @param boStopLossValue   BO stop-loss value
   * @param tag               Correlation ID for tracking
   * @param shouldSlice       Whether to use slicing endpoint
   */
  json placeOrder(const std::string &securityId,
                  const std::string &exchangeSegment,
                  const std::string &transactionType, int quantity,
                  const std::string &orderType, const std::string &productType,
                  double price, double triggerPrice = 0, int disclosedQty = 0,
                  bool afterMarketOrder = false,
                  const std::string &validity = "DAY",
                  const std::string &amoTime = "OPEN", double boProfitValue = 0,
                  double boStopLossValue = 0, const std::string &tag = "",
                  bool shouldSlice = false) {

    if (afterMarketOrder && amoTime != "OPEN" && amoTime != "OPEN_30" &&
        amoTime != "OPEN_60") {
      throw std::runtime_error(
          "amo_time must be one of: OPEN, OPEN_30, OPEN_60");
    }

    json payload = {{"dhanClientId", clientId_},
                    {"transactionType", toUpper(transactionType)},
                    {"exchangeSegment", toUpper(exchangeSegment)},
                    {"productType", toUpper(productType)},
                    {"orderType", toUpper(orderType)},
                    {"validity", toUpper(validity)},
                    {"securityId", securityId},
                    {"quantity", quantity},
                    {"disclosedQuantity", disclosedQty},
                    {"price", price},
                    {"afterMarketOrder", afterMarketOrder},
                    {"triggerPrice", triggerPrice}};

    if (boProfitValue > 0)
      payload["boProfitValue"] = boProfitValue;
    if (boStopLossValue > 0)
      payload["boStopLossValue"] = boStopLossValue;
    if (!tag.empty())
      payload["correlationId"] = tag;

    std::string endpoint = "/orders";
    if (shouldSlice)
      endpoint += "/slicing";

    json response = postRequest(endpoint, payload);
    std::cout << "placeOrder response: " << response.dump(2) << std::endl;
    return response;
  }

  /** Place a slice order (large qty auto-split). */
  json placeSliceOrder(
      const std::string &securityId, const std::string &exchangeSegment,
      const std::string &transactionType, int quantity,
      const std::string &orderType, const std::string &productType,
      double price, double triggerPrice = 0, int disclosedQty = 0,
      bool afterMarketOrder = false, const std::string &validity = "DAY",
      const std::string &amoTime = "OPEN", double boProfitValue = 0,
      double boStopLossValue = 0, const std::string &tag = "") {
    return placeOrder(securityId, exchangeSegment, transactionType, quantity,
                      orderType, productType, price, triggerPrice, disclosedQty,
                      afterMarketOrder, validity, amoTime, boProfitValue,
                      boStopLossValue, tag, true);
  }

  /** Modify a pending order. */
  json modifyOrder(const std::string &orderId, const std::string &orderType,
                   const std::string &legName, int quantity, double price,
                   double triggerPrice, int disclosedQty,
                   const std::string &validity) {
    json payload = {{"dhanClientId", clientId_},
                    {"orderId", orderId},
                    {"orderType", orderType},
                    {"legName", legName},
                    {"quantity", quantity},
                    {"price", price},
                    {"disclosedQuantity", disclosedQty},
                    {"triggerPrice", triggerPrice},
                    {"validity", validity}};

    json response = putRequest("/orders/" + orderId, payload);
    std::cout << "modifyOrder response: " << response.dump(2) << std::endl;
    return response;
  }

  /** Cancel a pending order. */
  json cancelOrder(const std::string &orderId) {
    json response = deleteRequest("/orders/" + orderId);
    std::cout << "cancelOrder response: " << response.dump(2) << std::endl;
    return response;
  }

  /** Get all orders for the day. */
  json getOrderList() {
    json response = getRequest("/orders");
    std::cout << "getOrderList response: " << response.dump(2) << std::endl;
    return response;
  }

  /** Get order by order ID. */
  json getOrderById(const std::string &orderId) {
    json response = getRequest("/orders/" + orderId);
    std::cout << "getOrderById response: " << response.dump(2) << std::endl;
    return response;
  }

  /** Get order by correlation ID. */
  json getOrderByCorrelationId(const std::string &correlationId) {
    json response = getRequest("/orders/external/" + correlationId);
    std::cout << "getOrderByCorrelationId response: " << response.dump(2)
              << std::endl;
    return response;
  }

  // ─── Forever Orders (GTT) ──────────────────

  /**
   * Place a forever order (GTT).
   *
   * @param orderFlag  "SINGLE" or "OCO"
   */
  json placeForeverOrder(
      const std::string &securityId, const std::string &exchangeSegment,
      const std::string &transactionType, const std::string &productType,
      const std::string &orderType, int quantity, double price,
      double triggerPrice, const std::string &orderFlag = "SINGLE",
      int disclosedQty = 0, const std::string &validity = "DAY",
      double price1 = 0, double triggerPrice1 = 0, int quantity1 = 0,
      const std::string &tag = "", const std::string &symbol = "") {

    json payload = {{"dhanClientId", clientId_},
                    {"orderFlag", orderFlag},
                    {"transactionType", toUpper(transactionType)},
                    {"exchangeSegment", toUpper(exchangeSegment)},
                    {"productType", toUpper(productType)},
                    {"orderType", toUpper(orderType)},
                    {"validity", toUpper(validity)},
                    {"tradingSymbol", symbol},
                    {"securityId", securityId},
                    {"quantity", quantity},
                    {"disclosedQuantity", disclosedQty},
                    {"price", price},
                    {"triggerPrice", triggerPrice},
                    {"price1", price1},
                    {"triggerPrice1", triggerPrice1},
                    {"quantity1", quantity1}};

    if (!tag.empty())
      payload["correlationId"] = tag;

    json response = postRequest("/forever/orders", payload);
    std::cout << "placeForeverOrder response: " << response.dump(2)
              << std::endl;
    return response;
  }

  /** Modify a forever order. */
  json modifyForeverOrder(const std::string &orderId,
                          const std::string &orderFlag,
                          const std::string &orderType,
                          const std::string &legName, int quantity,
                          double price, double triggerPrice, int disclosedQty,
                          const std::string &validity) {
    json payload = {{"dhanClientId", clientId_},
                    {"orderId", orderId},
                    {"orderFlag", orderFlag},
                    {"orderType", orderType},
                    {"legName", legName},
                    {"quantity", quantity},
                    {"disclosedQuantity", disclosedQty},
                    {"price", price},
                    {"triggerPrice", triggerPrice},
                    {"validity", validity}};

    json response = putRequest("/forever/orders/" + orderId, payload);
    std::cout << "modifyForeverOrder response: " << response.dump(2)
              << std::endl;
    return response;
  }

  /** Get all forever orders. */
  json getForeverOrders() {
    json response = getRequest("/forever/orders");
    std::cout << "getForeverOrders response: " << response.dump(2) << std::endl;
    return response;
  }

  /** Cancel a forever order. */
  json cancelForeverOrder(const std::string &orderId) {
    json response = deleteRequest("/forever/orders/" + orderId);
    std::cout << "cancelForeverOrder response: " << response.dump(2)
              << std::endl;
    return response;
  }

  // ─── Portfolio ──────────────────────────────

  /** Get all holdings. */
  json getHoldings() {
    json response = getRequest("/holdings");
    std::cout << "getHoldings response: " << response.dump(2) << std::endl;
    return response;
  }

  /** Get all open positions. */
  json getPositions() {
    json response = getRequest("/positions");
    std::cout << "getPositions response: " << response.dump(2) << std::endl;
    return response;
  }

  /** Convert position product type (e.g. INTRA → CNC). */
  json convertPosition(const std::string &fromProductType,
                       const std::string &exchangeSegment,
                       const std::string &positionType,
                       const std::string &securityId, int convertQty,
                       const std::string &toProductType) {
    json payload = {{"dhanClientId", clientId_},
                    {"fromProductType", fromProductType},
                    {"exchangeSegment", exchangeSegment},
                    {"positionType", positionType},
                    {"securityId", securityId},
                    {"convertQty", convertQty},
                    {"toProductType", toProductType}};

    json response = postRequest("/positions/convert", payload);
    std::cout << "convertPosition response: " << response.dump(2) << std::endl;
    return response;
  }

  // ─── Funds ──────────────────────────────────

  /** Get fund limits / balance info. */
  json getFundLimits() {
    json response = getRequest("/fundlimit");
    std::cout << "getFundLimits response: " << response.dump(2) << std::endl;
    return response;
  }

  /** Calculate margin required for a trade. */
  json marginCalculator(const std::string &securityId,
                        const std::string &exchangeSegment,
                        const std::string &transactionType, int quantity,
                        const std::string &productType, double price,
                        double triggerPrice = 0) {
    json payload = {{"dhanClientId", clientId_},
                    {"securityId", securityId},
                    {"exchangeSegment", toUpper(exchangeSegment)},
                    {"transactionType", toUpper(transactionType)},
                    {"quantity", quantity},
                    {"productType", toUpper(productType)},
                    {"price", price}};

    if (triggerPrice >= 0)
      payload["triggerPrice"] = triggerPrice;

    json response = postRequest("/margincalculator", payload);
    std::cout << "marginCalculator response: " << response.dump(2) << std::endl;
    return response;
  }

  // ─── Trader Controls ────────────────────────

  /**
   * Activate or deactivate kill switch.
   * @param action  "ACTIVATE" or "DEACTIVATE"
   */
  json killSwitch(const std::string &action) {
    std::string a = toUpper(action);
    if (a != "ACTIVATE" && a != "DEACTIVATE")
      throw std::runtime_error("action must be 'ACTIVATE' or 'DEACTIVATE'");

    json response =
        postRequest("/killswitch?killSwitchStatus=" + a, json::object());
    std::cout << "killSwitch response: " << response.dump(2) << std::endl;
    return response;
  }

  /** Get kill switch status. */
  json getKillSwitchStatus() {
    json response = getRequest("/killswitch");
    std::cout << "killSwitchStatus response: " << response.dump(2) << std::endl;
    return response;
  }

  // ─── Getters / Setters ──────────────────────

  std::string getClientId() const { return clientId_; }
  std::string getAccessToken() const { return accessToken_; }

  void setAccessToken(const std::string &token) { accessToken_ = token; }

private:
  std::string clientId_;
  std::string accessToken_;
  std::string baseUrl_;

  // ─── Utility ────────────────────────────────

  static std::string toUpper(const std::string &s) {
    std::string result = s;
    std::transform(result.begin(), result.end(), result.begin(), ::toupper);
    return result;
  }

  // ─── HTTP Helpers ───────────────────────────

  /**
   * Build Dhan headers.
   * Uses access-token and client-id custom headers.
   */
  struct curl_slist *buildHeaders(bool isJson = true) {
    struct curl_slist *headers = nullptr;

    if (isJson)
      headers = curl_slist_append(headers, "Content-Type: application/json");
    headers = curl_slist_append(headers, "Accept: application/json");

    std::string atHeader = "access-token: " + accessToken_;
    headers = curl_slist_append(headers, atHeader.c_str());

    std::string cidHeader = "client-id: " + clientId_;
    headers = curl_slist_append(headers, cidHeader.c_str());

    return headers;
  }

  /**
   * Parse response following Dhan's status/remarks/data format.
   */
  json parseResponse(const std::string &responseStr, long httpCode) {
    json result;
    try {
      json jsonResponse = json::parse(responseStr);

      if (httpCode >= 200 && httpCode <= 299) {
        result["status"] = "success";
        result["remarks"] = "";
        result["data"] = jsonResponse;
      } else {
        result["status"] = "failure";
        result["remarks"] = {
            {"error_code", jsonResponse.value("errorCode", "")},
            {"error_type", jsonResponse.value("errorType", "")},
            {"error_message", jsonResponse.value("errorMessage", "")}};
        result["data"] = "";
      }
    } catch (const json::parse_error &e) {
      result["status"] = "failure";
      result["remarks"] = e.what();
      result["data"] = "";
    }
    return result;
  }

  /** Execute HTTP request and return parsed response. */
  json httpRequest(const std::string &method, const std::string &path,
                   const std::string &body = "") {
    std::string url = baseUrl_ + path;

    CURL *curl = curl_easy_init();
    if (!curl)
      throw std::runtime_error("Failed to initialize libcurl");

    std::string responseStr;
    long httpCode = 0;
    bool isJson = (method == "POST" || method == "PUT");
    struct curl_slist *headers = buildHeaders(isJson);

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &responseStr);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);

    if (method == "POST") {
      curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
    } else if (method == "PUT") {
      curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PUT");
      curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
    } else if (method == "DELETE") {
      curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "DELETE");
    } else {
      // GET
      curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);
    }

    CURLcode res = curl_easy_perform(curl);
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK)
      throw std::runtime_error(std::string("HTTP request failed: ") +
                               curl_easy_strerror(res));

    return parseResponse(responseStr, httpCode);
  }

  /** GET request. */
  json getRequest(const std::string &path) { return httpRequest("GET", path); }

  /** POST JSON request. */
  json postRequest(const std::string &path, const json &payload) {
    json p = payload;
    p["dhanClientId"] = clientId_;
    return httpRequest("POST", path, p.dump());
  }

  /** PUT JSON request. */
  json putRequest(const std::string &path, const json &payload) {
    json p = payload;
    p["dhanClientId"] = clientId_;
    return httpRequest("PUT", path, p.dump());
  }

  /** DELETE request. */
  json deleteRequest(const std::string &path) {
    return httpRequest("DELETE", path);
  }
};

// ──────────────────────────────────────────────
//  main() – Usage demo
// ──────────────────────────────────────────────

// int main() {
//   curl_global_init(CURL_GLOBAL_DEFAULT);

//   try {
//     // Replace with your actual Dhan credentials
//     std::string clientId = "YOUR_CLIENT_ID";
//     std::string accessToken = "YOUR_ACCESS_TOKEN";

//     DhanBroker broker(clientId, accessToken);

//     // ── Example: Get fund limits / balance ──
//     std::cout << "\n=== Fund Limits ===" << std::endl;
//     json funds = broker.getFundLimits();
//     std::cout << funds.dump(2) << std::endl;

//     // ── Example: Place a market order ──
//     std::cout << "\n=== Place Order ===" << std::endl;
//     json orderResp = broker.placeOrder(
//         /* securityId      */ "1333",
//         /* exchangeSegment */ "NSE_EQ",
//         /* transactionType */ "BUY",
//         /* quantity        */ 1,
//         /* orderType       */ "MARKET",
//         /* productType     */ "INTRA",
//         /* price           */ 0);
//     std::cout << orderResp.dump(2) << std::endl;

//     // ── Example: Get order list ──
//     std::cout << "\n=== Order List ===" << std::endl;
//     json orders = broker.getOrderList();
//     std::cout << orders.dump(2) << std::endl;

//     // ── Example: Get positions ──
//     std::cout << "\n=== Positions ===" << std::endl;
//     json positions = broker.getPositions();
//     std::cout << positions.dump(2) << std::endl;

//     // ── Example: Get holdings ──
//     std::cout << "\n=== Holdings ===" << std::endl;
//     json holdings = broker.getHoldings();
//     std::cout << holdings.dump(2) << std::endl;

//     // ── Example: Margin calculator ──
//     std::cout << "\n=== Margin Calculator ===" << std::endl;
//     json margin =
//         broker.marginCalculator("1333", "NSE_EQ", "BUY", 1, "INTRA", 100.0);
//     std::cout << margin.dump(2) << std::endl;

//   } catch (const std::exception &e) {
//     std::cerr << "Error: " << e.what() << std::endl;
//     curl_global_cleanup();
//     return 1;
//   }

//   curl_global_cleanup();
//   return 0;
// }
