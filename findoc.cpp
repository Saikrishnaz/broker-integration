/**
 * Findoc B2C C++ Client
 *
 * Converted from findoc.py — the Findoc B2C REST API wrapper.
 * XTS-based API (similar to Symphony Fintech XTS).
 *
 * Dependencies:
 *   - libcurl       (HTTP requests)
 *   - nlohmann/json (JSON library)
 *
 * Compile:
 *   g++ -std=c++17 -o findoc findoc.cpp -lcurl
 *   (MSVC: cl /std:c++17 /EHsc findoc.cpp libcurl.lib)
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
//  FindocBroker Class
// ──────────────────────────────────────────────

class FindocBroker {
public:
  /**
   * Constructor
   *
   * @param clientId  Full client ID (last 4 chars used as payloadClientId)
   */
  FindocBroker(const std::string &clientId) : clientId_(clientId) {
    baseUrl_ = "https://xts.myfindoc.com";
    // Findoc uses last 4 chars of clientId in payloads
    if (clientId.size() >= 4)
      payloadClientId_ = clientId.substr(clientId.size() - 4);
    else
      payloadClientId_ = clientId;
    std::cout << "Findoc Broker initialized for clientId: " << clientId_
              << std::endl;
  }

  // ─── Authentication ─────────────────────────

  /**
   * Login and generate access token.
   *
   * @param secretKey  Secret key
   * @param apiKey     App key / API key
   * @param source     "WEBAPI" or "MOBILEAPI"
   * @return JSON with token etc.
   */
  json login(const std::string &secretKey, const std::string &apiKey,
             const std::string &source = "WEBAPI") {
    json payload = {
        {"appKey", apiKey}, {"secretKey", secretKey}, {"source", source}};

    json response = postRequest("/interactive/user/session", payload);
    std::cout << "login response: " << response.dump(2) << std::endl;

    if (response.contains("token")) {
      accessToken_ = response["token"].get<std::string>();
    }

    return response;
  }

  /** Logout and destroy session. */
  json logout() {
    json response = deleteRequest("/interactive/user/session");
    std::cout << "logout response: " << response.dump(2) << std::endl;
    accessToken_ = "";
    return response;
  }

  // ─── Balance / Profile ──────────────────────

  /** Get segment-wise balance. */
  json getBalance() {
    json response =
        getRequest("/interactive/user/balance?clientID=" + payloadClientId_);
    std::cout << "getBalance response: " << response.dump(2) << std::endl;
    return response;
  }

  /** Get user profile. */
  json getProfile() {
    json response =
        getRequest("/interactive/user/profile?clientID=" + payloadClientId_);
    std::cout << "getProfile response: " << response.dump(2) << std::endl;
    return response;
  }

  // ─── Orders ─────────────────────────────────

  /**
   * Place an order.
   *
   * @param exchange          "NSECM", "BSECM", "NSEFO", "BSEFO", "MCXFO"
   * @param exchangeToken     Exchange instrument ID (int)
   * @param transactionType   "BUY" or "SELL"
   * @param productType       "MIS", "NRML", "CNC"
   * @param orderType         "LIMIT" or "MARKET"
   * @param quantity          Order quantity
   * @param price             Limit price (0 for market)
   * @param stopPrice         Stop/trigger price (0 if none)
   * @param disclosedQty      Disclosed quantity (0 if none)
   * @param validity          "DAY", "IOC"
   * @param orderTag          Optional unique order identifier
   */
  json placeOrder(const std::string &exchange, int exchangeToken,
                  const std::string &transactionType,
                  const std::string &productType, const std::string &orderType,
                  int quantity, double price = 0, double stopPrice = 0,
                  int disclosedQty = 0, const std::string &validity = "DAY",
                  const std::string &orderTag = "") {
    json payload = {{"exchangeSegment", exchange},
                    {"exchangeInstrumentID", exchangeToken},
                    {"productType", productType},
                    {"orderType", orderType},
                    {"orderSide", transactionType},
                    {"timeInForce", validity},
                    {"disclosedQuantity", disclosedQty},
                    {"orderQuantity", quantity},
                    {"limitPrice", price},
                    {"stopPrice", stopPrice},
                    {"clientID", payloadClientId_}};

    if (!orderTag.empty())
      payload["orderUniqueIdentifier"] = orderTag;

    json response = postRequest("/interactive/orders", payload);
    std::cout << "placeOrder response: " << response.dump(2) << std::endl;
    return response;
  }

  /**
   * Modify a pending order.
   *
   * @param orderId       App order ID to modify
   * @param productType   New product type
   * @param quantity      New quantity
   * @param orderType     New order type ("LIMIT", "MARKET")
   * @param price         New limit price
   * @param stopPrice     New stop price
   * @param disclosedQty  New disclosed quantity
   * @param orderTag      Optional unique identifier
   */
  json modifyOrder(const std::string &orderId, const std::string &productType,
                   int quantity, const std::string &orderType, double price,
                   double stopPrice = 0, int disclosedQty = 0,
                   const std::string &orderTag = "") {
    json payload = {{"appOrderID", orderId},
                    {"modifiedProductType", productType},
                    {"modifiedOrderType", orderType},
                    {"modifiedOrderQuantity", quantity},
                    {"modifiedDisclosedQuantity", disclosedQty},
                    {"modifiedLimitPrice", price},
                    {"modifiedStopPrice", stopPrice},
                    {"modifiedTimeInForce", "DAY"},
                    {"clientID", payloadClientId_}};

    if (!orderTag.empty())
      payload["orderUniqueIdentifier"] = orderTag;
    else
      payload["orderUniqueIdentifier"] = "024870146";

    json response = putRequest("/interactive/orders", payload);
    std::cout << "modifyOrder response: " << response.dump(2) << std::endl;
    return response;
  }

  /** Cancel a specific order by appOrderID. */
  json cancelOrder(const std::string &orderId) {
    std::string url = "/interactive/orders?appOrderID=" + orderId +
                      "&clientID=" + payloadClientId_;
    json response = deleteRequest(url);
    std::cout << "cancelOrder response: " << response.dump(2) << std::endl;
    return response;
  }

  /**
   * Cancel all orders for an exchange segment.
   *
   * @param exchange       "NSECM", "BSECM", "NSEFO", "BSEFO", "MCXFO"
   * @param exchangeToken  Exchange instrument ID (0 for all)
   */
  json cancelAllOrders(const std::string &exchange, int exchangeToken = 0) {
    json payload = {{"exchangeSegment", exchange},
                    {"exchangeInstrumentID", exchangeToken}};
    json response = postRequest("/interactive/orders/cancelall", payload);
    std::cout << "cancelAllOrders response: " << response.dump(2) << std::endl;
    return response;
  }

  // ─── Order Book / History ───────────────────

  /** Get full order history. */
  json getOrderHistory() {
    json response =
        getRequest("/interactive/orders?clientID=" + payloadClientId_);
    std::cout << "getOrderHistory response: " << response.dump(2) << std::endl;
    return response;
  }

  /** Get order history for a specific order ID. */
  json getOrderHistoryById(const std::string &orderId) {
    std::string url = "/interactive/orders?appOrderID=" + orderId +
                      "clientID=" + payloadClientId_;
    json response = getRequest(url);
    std::cout << "getOrderHistoryById response: " << response.dump(2)
              << std::endl;
    return response;
  }

  // ─── Portfolio ──────────────────────────────

  /**
   * Get positions.
   * @param type  "DayWise" or "NetWise"
   */
  json getPositions(const std::string &type = "NetWise") {
    std::string url = "/interactive/portfolio/positions?dayOrNet=" + type +
                      "clientID=" + payloadClientId_;
    json response = getRequest(url);
    std::cout << "getPositions response: " << response.dump(2) << std::endl;
    return response;
  }

  // ─── Getters / Setters ──────────────────────

  std::string getAccessToken() const { return accessToken_; }
  std::string getClientId() const { return clientId_; }

  void setAccessToken(const std::string &token) { accessToken_ = token; }

private:
  std::string clientId_;
  std::string payloadClientId_; // last 4 chars of clientId
  std::string baseUrl_;
  std::string accessToken_;

  // ─── HTTP Helpers ───────────────────────────

  /**
   * Build Findoc headers.
   * Authorization = raw token (no Bearer prefix).
   */
  struct curl_slist *buildHeaders(bool isJson = true) {
    struct curl_slist *headers = nullptr;

    if (isJson)
      headers = curl_slist_append(headers, "Content-Type: application/json");

    if (!accessToken_.empty()) {
      std::string auth = "Authorization: " + accessToken_;
      headers = curl_slist_append(headers, auth.c_str());
    }

    return headers;
  }

  /** Parse response JSON, extracting "result" if present. */
  json parseResponse(const std::string &responseStr) {
    try {
      json data = json::parse(responseStr);
      if (data.contains("result"))
        return data["result"];
      return data;
    } catch (const json::parse_error &e) {
      std::cerr << "JSON parse error: " << e.what() << std::endl;
      std::cerr << "Raw response: " << responseStr << std::endl;
      throw;
    }
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

  /** POST JSON request. */
  json postRequest(const std::string &path, const json &payload) {
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

  /** PUT JSON request (used for modify order). */
  json putRequest(const std::string &path, const json &payload) {
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

  /** DELETE request (used for logout and cancel order). */
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
// // ──────────────────────────────────────────────

// int main() {
//   curl_global_init(CURL_GLOBAL_DEFAULT);

//   try {
//     // Replace with your actual Findoc credentials
//     std::string clientId = "YOUR_CLIENT_ID";

//     FindocBroker broker(clientId);

//     // ── Step 1: Login ──
//     std::cout << "\n=== Login ===" << std::endl;
//     json loginResp = broker.login(
//         /* secretKey */ "YOUR_SECRET_KEY",
//         /* apiKey    */ "YOUR_API_KEY");
//     std::cout << loginResp.dump(2) << std::endl;
//     std::cout << "Token: " << broker.getAccessToken() << std::endl;

//     // ── Example: Get balance ──
//     std::cout << "\n=== Balance ===" << std::endl;
//     json balance = broker.getBalance();
//     std::cout << balance.dump(2) << std::endl;

//     // ── Example: Get profile ──
//     std::cout << "\n=== Profile ===" << std::endl;
//     json profile = broker.getProfile();
//     std::cout << profile.dump(2) << std::endl;

//     // ── Example: Place a market order ──
//     std::cout << "\n=== Place Order ===" << std::endl;
//     json orderResp = broker.placeOrder(
//         /* exchange        */ "NSEFO",
//         /* exchangeToken   */ 5866,
//         /* transactionType */ "BUY",
//         /* productType     */ "NRML",
//         /* orderType       */ "MARKET",
//         /* quantity        */ 1);
//     std::cout << orderResp.dump(2) << std::endl;

//     // ── Example: Get order history ──
//     std::cout << "\n=== Order History ===" << std::endl;
//     json orderHistory = broker.getOrderHistory();
//     std::cout << orderHistory.dump(2) << std::endl;

//     // ── Example: Get positions ──
//     std::cout << "\n=== Positions ===" << std::endl;
//     json positions = broker.getPositions("NetWise");
//     std::cout << positions.dump(2) << std::endl;

//   } catch (const std::exception &e) {
//     std::cerr << "Error: " << e.what() << std::endl;
//     curl_global_cleanup();
//     return 1;
//   }

//   curl_global_cleanup();
//   return 0;
// }
