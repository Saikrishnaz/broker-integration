/**
 * Motilal Oswal (MOFSLOPENAPI) C++ Client
 *
 * Converted from MOFSLOPENAPI.py — the official Motilal Oswal
 * Open API wrapper.
 *
 * Dependencies:
 *   - libcurl       (HTTP requests)
 *   - nlohmann/json (JSON library)
 *
 * Compile:
 *   g++ -std=c++17 -o motilal motilal.cpp -lcurl
 *   (MSVC: cl /std:c++17 /EHsc motilal.cpp libcurl.lib)
 */

#include <algorithm>
#include <cstring>
#include <iostream>
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
//  MotilalOswalBroker Class
// ──────────────────────────────────────────────

class MotilalOswalBroker {
public:
  /**
   * Constructor
   *
   * @param apiKey       API key
   * @param baseUrl      Base URL (e.g. "https://openapi.motilaloswal.com")
   * @param clientCode   Client / dealer code
   * @param authToken    Pre-obtained auth token
   * @param sourceId     "WEB", "Desktop", or "Mobile"
   * @param browserName  Browser name (for WEB source)
   * @param browserVer   Browser version (for WEB source)
   */
  MotilalOswalBroker(const std::string &apiKey,
                     const std::string &clientCode,
                     const std::string &authToken,
                     const std::string &sourceId = "WEB",
                     const std::string &browserName = "Chrome",
                     const std::string &browserVer = "120.0")
      : apiKey_(apiKey), clientCode_(clientCode),
        authToken_(authToken), sourceId_(sourceId), browserName_(browserName),
          browserVersion_(browserVer) {
    const std::string baseUrl  = "https://openapi.motilaloswal.com";
    std::cout << "Motilal Oswal Broker initialized with base URL: " << baseUrl << std::endl;
  }

  // ─── User Profile ───────────────────────────

  /** Get user profile. */
  json getProfile(const std::string &clientCode = "") {
    std::string code = clientCode.empty() ? clientCode_ : clientCode;
    json payload = {{"clientcode", code}};
    json response = postRequest("/rest/login/v1/getprofile", payload);
    std::cout << "getProfile response: " << response.dump(2) << std::endl;
    return response;
  }

  // ─── Orders ─────────────────────────────────

  /**
   * Place an order.
   * Pass the complete order payload as JSON.
   *
   * Typical fields: clientcode, exchange, symboltoken, buyorsell,
   * ordertype, producttype, orderduration, price, triggerprice,
   * quantity, disclosedquantity, amoorder, tag, etc.
   */
  json placeOrder(const json &orderInfo) {
    json response = postRequest("/rest/trans/v1/placeorder", orderInfo);
    std::cout << "placeOrder response: " << response.dump(2) << std::endl;
    return response;
  }

  /** Modify an existing order. */
  json modifyOrder(const json &modifyInfo) {
    json response = postRequest("/rest/trans/v2/modifyorder", modifyInfo);
    std::cout << "modifyOrder response: " << response.dump(2) << std::endl;
    return response;
  }

  /** Cancel an order. */
  json cancelOrder(const std::string &uniqueOrderId,
                   const std::string &clientCode = "") {
    std::string code = clientCode.empty() ? clientCode_ : clientCode;
    json payload = {{"clientcode", code}, {"uniqueorderid", uniqueOrderId}};
    json response = postRequest("/rest/trans/v1/cancelorder", payload);
    std::cout << "cancelOrder response: " << response.dump(2) << std::endl;
    return response;
  }

  // ─── Order & Trade Book ─────────────────────

  /** Get order book. */
  json getOrderBook(const json &orderBookInfo) {
    json response = postRequest("/rest/book/v1/getorderbook", orderBookInfo);
    std::cout << "getOrderBook response: " << response.dump(2) << std::endl;
    return response;
  }

  /** Get trade book. */
  json getTradeBook(const std::string &clientCode = "") {
    std::string code = clientCode.empty() ? clientCode_ : clientCode;
    json payload = {{"clientcode", code}};
    json response = postRequest("/rest/book/v1/gettradebook", payload);
    std::cout << "getTradeBook response: " << response.dump(2) << std::endl;
    return response;
  }

  /** Get order detail by unique order ID. */
  json getOrderDetailByUniqueOrderId(const std::string &uniqueOrderId,
                                     const std::string &clientCode = "") {
    std::string code = clientCode.empty() ? clientCode_ : clientCode;
    json payload = {{"clientcode", code}, {"uniqueorderid", uniqueOrderId}};
    json response =
        postRequest("/rest/book/v1/getorderdetailbyuniqueorderid", payload);
    std::cout << "getOrderDetail response: " << response.dump(2) << std::endl;
    return response;
  }

  // ─── Portfolio ──────────────────────────────

  /** Get positions. */
  json getPositions(const std::string &clientCode = "") {
    std::string code = clientCode.empty() ? clientCode_ : clientCode;
    json payload = {{"clientcode", code}};
    json response = postRequest("/rest/book/v1/getposition", payload);
    std::cout << "getPositions response: " << response.dump(2) << std::endl;
    return response;
  }

  /** Get DP holdings. */
  json getDPHolding(const std::string &clientCode = "") {
    std::string code = clientCode.empty() ? clientCode_ : clientCode;
    json payload = {{"clientcode", code}};
    json response = postRequest("/rest/report/v1/getdpholding", payload);
    std::cout << "getDPHolding response: " << response.dump(2) << std::endl;
    return response;
  }

  /** Convert position product type. */
  json positionConversion(const json &conversionInfo) {
    json response =
        postRequest("/rest/trans/v1/positionconversion", conversionInfo);
    std::cout << "positionConversion response: " << response.dump(2)
              << std::endl;
    return response;
  }

  // ─── Reports / Margin ───────────────────────

  /** Get margin report. */
  json getReportMargin(const std::string &clientCode = "") {
    std::string code = clientCode.empty() ? clientCode_ : clientCode;
    json payload = {{"clientcode", code}};
    json response = postRequest("/rest/report/v1/getreportmargin", payload);
    std::cout << "getReportMargin response: " << response.dump(2) << std::endl;
    return response;
  }

  /** Get margin summary. */
  json getReportMarginSummary(const std::string &clientCode = "") {
    std::string code = clientCode.empty() ? clientCode_ : clientCode;
    json payload = {{"clientcode", code}};
    json response =
        postRequest("/rest/report/v1/getreportmarginsummary", payload);
    std::cout << "getReportMarginSummary response: " << response.dump(2)
              << std::endl;
    return response;
  }

  /** Get margin detail. */
  json getReportMarginDetail(const std::string &clientCode = "") {
    std::string code = clientCode.empty() ? clientCode_ : clientCode;
    json payload = {{"clientcode", code}};
    json response =
        postRequest("/rest/report/v1/getreportmargindetail", payload);
    std::cout << "getReportMarginDetail response: " << response.dump(2)
              << std::endl;
    return response;
  }

  /** Get brokerage detail. */
  json getBrokerageDetail(const json &brokerageInfo) {
    json response =
        postRequest("/rest/report/v1/getbrokeragedetail", brokerageInfo);
    std::cout << "getBrokerageDetail response: " << response.dump(2)
              << std::endl;
    return response;
  }

  // ─── Market Data ────────────────────────────

  /** Get LTP data. */
  json getLtp(const json &ltpData) {
    json response = postRequest("/rest/report/v1/getltpdata", ltpData);
    std::cout << "getLtp response: " << response.dump(2) << std::endl;
    return response;
  }

  /** Get instrument / scrip data by exchange name. */
  json getInstrumentFile(const std::string &exchangeName,
                         const std::string &clientCode = "") {
    std::string code = clientCode.empty() ? clientCode_ : clientCode;
    json payload = {{"clientcode", code}, {"exchangename", exchangeName}};
    json response =
        postRequest("/rest/report/v1/getscripsbyexchangename", payload);
    std::cout << "getInstrumentFile response: " << response.dump(2)
              << std::endl;
    return response;
  }

  /** Get broadcast max limit. */
  json getBroadcastMaxLimit(const std::string &clientCode = "") {
    std::string code = clientCode.empty() ? clientCode_ : clientCode;
    json payload = {{"clientcode", code}};
    json response =
        postRequest("/rest/report/v1/getbroadcastmaxlimit", payload);
    std::cout << "getBroadcastMaxLimit response: " << response.dump(2)
              << std::endl;
    return response;
  }

  // ─── Getters / Setters ──────────────────────

  std::string getAuthToken() const { return authToken_; }
  std::string getClientCode() const { return clientCode_; }

  void setAuthToken(const std::string &token) { authToken_ = token; }
  void setApiSecretKey(const std::string &key) { apiSecretKey_ = key; }

private:
  std::string apiKey_;
  std::string apiSecretKey_;
  std::string baseUrl_;
  std::string clientCode_;
  std::string authToken_;
  std::string sourceId_;
  std::string vendorInfo_;
  std::string browserName_;
  std::string browserVersion_;

  // Client metadata (matching Python MOFSLOPENAPI)
  std::string userAgent_ = "MOSL/V.1.1.0";
  std::string macAddress_ = "00:00:00:00:00:00";
  std::string clientLocalIP_ = "127.0.0.1";
  std::string clientPublicIP_ = "1.2.3.4";
  std::string osName_ = "Windows";
  std::string osVersion_ = "10.0";
  std::string installedAppId_ = "00000000-0000-0000-0000-000000000000";
  std::string deviceModel_ = "Desktop";
  std::string manufacturer_ = "unknown";
  std::string productName_ = "Investor";
  std::string productVersion_ = "1";
  std::string latitude_ = "0.0000";
  std::string longitude_ = "0.0000";

  // ─── HTTP Helpers ───────────────────────────

  /**
   * Build Motilal Oswal headers.
   * Matches the extensive header set from Python validate() method.
   */
  struct curl_slist *buildHeaders() {
    struct curl_slist *headers = nullptr;

    headers = curl_slist_append(headers, "Content-Type: application/json");
    headers = curl_slist_append(headers, "Accept: application/json");

    // Auth token
    std::string auth = "Authorization: " + authToken_;
    headers = curl_slist_append(headers, auth.c_str());

    // User-Agent
    std::string ua = "User-Agent: " + userAgent_;
    headers = curl_slist_append(headers, ua.c_str());

    // API keys
    std::string ak = "apikey: " + apiKey_;
    headers = curl_slist_append(headers, ak.c_str());

    std::string ask = "apisecretkey: " + apiSecretKey_;
    headers = curl_slist_append(headers, ask.c_str());

    // Client info
    std::string mac = "macaddress: " + macAddress_;
    headers = curl_slist_append(headers, mac.c_str());

    std::string lip = "clientlocalip: " + clientLocalIP_;
    headers = curl_slist_append(headers, lip.c_str());

    std::string sid = "sourceid: " + sourceId_;
    headers = curl_slist_append(headers, sid.c_str());

    std::string pip = "clientpublicip: " + clientPublicIP_;
    headers = curl_slist_append(headers, pip.c_str());

    std::string vi = "vendorinfo: " + vendorInfo_;
    headers = curl_slist_append(headers, vi.c_str());

    // System info headers
    std::string on = "osname: " + osName_;
    headers = curl_slist_append(headers, on.c_str());

    std::string ov = "osversion: " + osVersion_;
    headers = curl_slist_append(headers, ov.c_str());

    std::string ai = "installedappid: " + installedAppId_;
    headers = curl_slist_append(headers, ai.c_str());

    std::string dm = "devicemodel: " + deviceModel_;
    headers = curl_slist_append(headers, dm.c_str());

    std::string mf = "manufacturer: " + manufacturer_;
    headers = curl_slist_append(headers, mf.c_str());

    std::string pn = "productname: " + productName_;
    headers = curl_slist_append(headers, pn.c_str());

    std::string pv = "productversion: " + productVersion_;
    headers = curl_slist_append(headers, pv.c_str());

    std::string lat = "latitude: " + latitude_;
    headers = curl_slist_append(headers, lat.c_str());

    std::string lon = "longitude: " + longitude_;
    headers = curl_slist_append(headers, lon.c_str());

    headers = curl_slist_append(headers, "sdkversion: CPP 1.0");

    // Browser headers for WEB source
    if (sourceId_ == "WEB" || sourceId_ == "web") {
      std::string bn = "browsername: " + browserName_;
      headers = curl_slist_append(headers, bn.c_str());

      std::string bv = "browserversion: " + browserVersion_;
      headers = curl_slist_append(headers, bv.c_str());
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

  /** POST JSON request (all MOFSL APIs use POST). */
  json postRequest(const std::string &path, const json &payload) {
    std::string url = baseUrl_ + path;
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

    CURLcode res = curl_easy_perform(curl);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK)
      throw std::runtime_error(std::string("HTTP POST failed: ") +
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
//     // Replace with your actual Motilal Oswal credentials
//     std::string apiKey = "lQDa81JYve1EPWMm";
//     std::string baseUrl = "https://openapi.motilaloswal.com";
//     std::string clientCode = "EMUM503065";
//     std::string authToken =
//         "cdeac554d66f4ae9b6f4b579f8a55e24_M"; // Provide your auth token directly

//     MotilalOswalBroker broker(apiKey, baseUrl, clientCode, authToken);

//     // ── Example: Get profile ──
//     std::cout << "\n=== Profile ===" << std::endl;
//     json profile = broker.getProfile();
//     std::cout << profile.dump(2) << std::endl;

//     // ── Example: Get margin report ──
//     std::cout << "\n=== Margin Report ===" << std::endl;
//     json margin = broker.getReportMargin();
//     std::cout << margin.dump(2) << std::endl;

//     // ── Example: Place order ──
//     std::cout << "\n=== Place Order ===" << std::endl;
//     json orderInfo = {{"clientcode", clientCode}, {"exchange", "NSEFO"},
//                       {"symboltoken", 35229},    {"buyorsell", "BUY"},
//                       {"ordertype", "LIMIT"},       {"producttype", "VALUEPLUS"},
//                       {"orderduration", "DAY"},   {"price", 20.5},
//                       {"triggerprice", 0},      {"quantityinlot", 1},
//                       {"disclosedquantity", 0}, {"amoorder", "N"},
//                        {"algoid",""},           {"tag","Helo"}};
//     json orderResp = broker.placeOrder(orderInfo);
//     std::cout << orderResp.dump(2) << std::endl;

//     // ── Example: Get order book ──
//     std::cout << "\n=== Order Book ===" << std::endl;
//     json obInfo = {{"clientcode", clientCode}};
//     json orderBook = broker.getOrderBook(obInfo);
//     std::cout << orderBook.dump(2) << std::endl;

//     // ── Example: Get positions ──
//     std::cout << "\n=== Positions ===" << std::endl;
//     json positions = broker.getPositions();
//     std::cout << positions.dump(2) << std::endl;

//     // ── Example: Get DP holdings ──
//     std::cout << "\n=== DP Holdings ===" << std::endl;
//     json holdings = broker.getDPHolding();
//     std::cout << holdings.dump(2) << std::endl;

//     // ── Example: Get LTP ──
//     std::cout << "\n=== LTP ===" << std::endl;
//     json ltpData = {{"clientcode", clientCode},
//                     {"exchange", "NSE"},
//                     {"symboltoken", "3045"}};
//     json ltp = broker.getLtp(ltpData);
//     std::cout << ltp.dump(2) << std::endl;

//   } catch (const std::exception &e) {
//     std::cerr << "Error: " << e.what() << std::endl;
//     curl_global_cleanup();
//     return 1;
//   }

//   curl_global_cleanup();
//   return 0;
// }
