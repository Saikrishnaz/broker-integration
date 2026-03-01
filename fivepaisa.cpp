/**
 * 5paisa Broker C++ Client
 *
 * Converted from fivepaisa.py - provides a class for interacting with the
 * 5paisa broker API.
 *
 * Dependencies:
 *   - libcurl    (HTTP requests)
 *   - nlohmann/json (json.hpp, single-header JSON library)
 *   - OpenSSL    (HMAC-SHA1 for TOTP generation)
 *
 * Compile:
 *   g++ -std=c++17 -o fivepaisa fivepaisa.cpp -lcurl -lssl -lcrypto
 *   (On Windows with MSVC: cl /std:c++17 fivepaisa.cpp libcurl.lib libssl.lib
 * libcrypto.lib)
 */

#include <algorithm>
#include <cmath>
#include <cstring>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>


#include <curl/curl.h>
#include <openssl/evp.h>
#include <openssl/hmac.h>


#include <nlohmann/json.hpp>

using json = nlohmann::json;

// ──────────────────────────────────────────────
//  TOTP Generator (RFC 6238 / RFC 4226)
// ──────────────────────────────────────────────

class TOTPGenerator {
public:
  explicit TOTPGenerator(const std::string &base32Secret, int digits = 6,
                         int period = 30)
      : digits_(digits), period_(period) {
    key_ = base32Decode(base32Secret);
  }

  std::string now() const {
    uint64_t counter = static_cast<uint64_t>(std::time(nullptr)) / period_;
    return generate(counter);
  }

private:
  std::vector<uint8_t> key_;
  int digits_;
  int period_;

  static std::vector<uint8_t> base32Decode(const std::string &encoded) {
    static const std::string alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZ234567";
    std::vector<uint8_t> result;
    int buffer = 0, bitsLeft = 0;

    for (char c : encoded) {
      if (c == '=' || c == ' ')
        continue;
      auto pos = alphabet.find(static_cast<char>(toupper(c)));
      if (pos == std::string::npos)
        throw std::runtime_error("Invalid Base32 character");

      buffer = (buffer << 5) | static_cast<int>(pos);
      bitsLeft += 5;
      if (bitsLeft >= 8) {
        bitsLeft -= 8;
        result.push_back(static_cast<uint8_t>((buffer >> bitsLeft) & 0xFF));
      }
    }
    return result;
  }

  std::string generate(uint64_t counter) const {
    // Convert counter to big-endian 8 bytes
    uint8_t msg[8];
    for (int i = 7; i >= 0; --i) {
      msg[i] = static_cast<uint8_t>(counter & 0xFF);
      counter >>= 8;
    }

    // HMAC-SHA1
    unsigned int hmacLen = 0;
    unsigned char hmacResult[EVP_MAX_MD_SIZE];
    HMAC(EVP_sha1(), key_.data(), static_cast<int>(key_.size()), msg, 8,
         hmacResult, &hmacLen);

    // Dynamic truncation (RFC 4226)
    int offset = hmacResult[hmacLen - 1] & 0x0F;
    uint32_t code = ((hmacResult[offset] & 0x7F) << 24) |
                    ((hmacResult[offset + 1] & 0xFF) << 16) |
                    ((hmacResult[offset + 2] & 0xFF) << 8) |
                    ((hmacResult[offset + 3] & 0xFF));

    uint32_t mod = 1;
    for (int i = 0; i < digits_; ++i)
      mod *= 10;
    code %= mod;

    std::ostringstream oss;
    oss << std::setfill('0') << std::setw(digits_) << code;
    return oss.str();
  }
};

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
//  FivepaisaBroker Class
// ──────────────────────────────────────────────

class FivepaisaBroker {
public:
  /**
   * Constructor – mirrors Python __init__
   *
   * @param userId        User ID for authentication
   * @param userKey       User key (goes in payload head)
   * @param appPassword   Application password
   * @param encryptionKey Encryption key for access token
   * @param clientCode    Client code / email
   * @param pin           PIN for TOTP login
   * @param totpKey       Base32-encoded TOTP secret
   * @param appName       (optional) application name
   */
  FivepaisaBroker(const std::string &userId, const std::string &userKey,
                  const std::string &appPassword,
                  const std::string &encryptionKey,
                  const std::string &clientCode, const std::string &pin,
                  const std::string &totpKey, const std::string &appName = "")
      : userId_(userId), userKey_(userKey), appPassword_(appPassword),
        encryptionKey_(encryptionKey), clientCode_(clientCode), pin_(pin),
        totp_(totpKey), appName_(appName), authenticated_(false) {
    // Build base generic payload
    genericPayload_ = {{"head", {{"Key", userKey_}}}, {"body", json::object()}};

    headers_["Content-Type"] = "application/json";

    // URLs
    baseUrl_ = "https://Openapi.5paisa.com/VendorsAPI/Service1.svc/";
    loginRoute_ = baseUrl_ + "V4/LoginRequestMobileNewbyEmail";
    scripMasterRoute_ = baseUrl_ + "ScripMaster/segment/All";
    orderBookRoute_ = baseUrl_ + "V3/OrderBook";
    holdingsRoute_ = baseUrl_ + "V3/Holding";
    positionsRoute_ = baseUrl_ + "V2/NetPositionNetWise";
    orderPlacementRoute_ = baseUrl_ + "V1/PlaceOrderRequest";
    orderModifyRoute_ = baseUrl_ + "V1/ModifyOrderRequest";
    orderCancelRoute_ = baseUrl_ + "V1/CancelOrderRequest";
    orderStatusRoute_ = baseUrl_ + "V2/OrderStatus";
    tradeInfoRoute_ = baseUrl_ + "TradeInformation";
    jwtValidationRoute_ = "https://Openapi.indiainfoline.com/VendorsAPI/"
                          "Service1.svc/JWTOpenApiValidation";
    historicalDataRoute_ = "https://openapi.5paisa.com/V2/historical/";
    getRequestTokenRoute_ = baseUrl_ + "TOTPLogin";
    accessTokenRoute_ = baseUrl_ + "GetAccessToken";
    netPositionRoute_ = baseUrl_ + "V2/NetPositionNetWise";
    multiOrderMarginRoute_ = baseUrl_ + "MultiOrderMargin";

    std::cout << "Initializing 5paisa Broker for client: " << clientCode_
              << std::endl;
  }

  // ─── Authentication ───────────────────────

  void generateRequestToken() {
    json payload = genericPayload_;
    payload["body"] = {
        {"Email_ID", clientCode_}, {"TOTP", totp_.now()}, {"PIN", pin_}};

    json response = postJson(getRequestTokenRoute_, payload);
    std::cout << "response === " << response.dump(2) << std::endl;

    if (response.contains("body") && !response["body"].is_null() &&
        response["body"].contains("Message")) {
      std::string msg = response["body"]["Message"].get<std::string>();
      std::transform(msg.begin(), msg.end(), msg.begin(), ::tolower);
      if (msg == "success") {
        requestToken_ = response["body"]["RequestToken"].get<std::string>();
      } else {
        throw std::runtime_error(
            response["body"]["Message"].get<std::string>());
      }
    } else {
      if (response.contains("head") && !response["head"].is_null() &&
          response["head"].contains("StatusDescription")) {
        throw std::runtime_error(
            response["head"]["StatusDescription"].get<std::string>());
      }
      throw std::runtime_error("failed to generate request token");
    }
  }

  void generateAccessToken() {
    if (requestToken_.empty()) {
      generateRequestToken();
    }

    json payload = genericPayload_;
    payload["body"] = {{"RequestToken", requestToken_},
                       {"EncryKey", encryptionKey_},
                       {"UserId", userId_}};
    std::cout << "payload ==== " << payload.dump(2) << std::endl;

    json response = postJson(accessTokenRoute_, payload);
    std::cout << "response(json) ==== " << response.dump(2) << std::endl;

    if (response.contains("body") && !response["body"].is_null() &&
        response["body"].contains("Message")) {
      std::string msg = response["body"]["Message"].get<std::string>();
      std::transform(msg.begin(), msg.end(), msg.begin(), ::tolower);
      if (msg == "success") {
        accessToken_ = response["body"]["AccessToken"].get<std::string>();
        headers_["Authorization"] = "bearer " + accessToken_;
      } else {
        throw std::runtime_error(
            response["body"]["Message"].get<std::string>());
      }
    } else {
      if (response.contains("head") && !response["head"].is_null() &&
          response["head"].contains("StatusDescription")) {
        throw std::runtime_error(
            response["head"]["StatusDescription"].get<std::string>());
      }
      throw std::runtime_error("failed to generate access token");
    }
  }

  void authenticate() {
    generateAccessToken();
    authenticated_ = true;
  }

  // ─── Order Placement ──────────────────────

  /**
   * Place an order.
   *
   * @param scripCode       Exchange token / scrip code
   * @param exchange        "N" (NSE), "B" (BSE), "M" (MCX)
   * @param exchangeType    "C" (Cash), "D" (Derivatives), "U" (Currency)
   * @param transactionType "B" (Buy), "S" (Sell)
   * @param quantity        Quantity
   * @param productType     "NRML" or "MIS"
   * @param orderType       "MARKET" or "LIMIT"
   * @param orderTag        Unique client reference (optional)
   * @param price           Order price (0 for MARKET)
   * @param disQty          Disclosed quantity (optional)
   * @param stopLossPrice   Stop-loss price (optional)
   * @return json response body
   */
  json placeOrder(const std::string &scripCode, const std::string &exchange,
                  const std::string &exchangeType,
                  const std::string &transactionType, int quantity,
                  const std::string &productType, const std::string &orderType,
                  const std::string &orderTag = "", double price = 0.0,
                  int disQty = 0, double stopLossPrice = 0.0) {
    // Validate inputs
    if (productType != "NRML" && productType != "MIS")
      throw std::runtime_error("only NRML and MIS order are allowed");
    if (exchange != "N" && exchange != "B" && exchange != "M")
      throw std::runtime_error("only N, B and M exchange are allowed");
    if (transactionType != "B" && transactionType != "S")
      throw std::runtime_error("only B and S transaction type are allowed");

    std::string ot = orderType;
    std::transform(ot.begin(), ot.end(), ot.begin(), ::toupper);
    if (ot != "MARKET" && ot != "LIMIT")
      throw std::runtime_error("only MARKET and LIMIT Order type are allowed");

    json payload = genericPayload_;
    payload["body"] = {{"OrderType", transactionType},
                       {"Exchange", exchange},
                       {"ExchangeType", exchangeType},
                       {"ScripCode", scripCode},
                       {"ScripData", ""},
                       {"Price", std::to_string(price)},
                       {"Qty", std::to_string(quantity)},
                       {"StopLossPrice", std::to_string(stopLossPrice)},
                       {"DisQty", disQty},
                       {"IsIntraday", false},
                       {"AHPlaced", "N"},
                       {"RemoteOrderID", orderTag}};

    if (ot == "MARKET") {
      payload["body"]["Price"] = 0;
    } else if (ot == "LIMIT" && price <= 0) {
      throw std::runtime_error("price can't be 0 for LIMIT order");
    }

    json response = postJson(orderPlacementRoute_, payload, true);
    std::cout << "response(json) == " << response.dump(2) << std::endl;

    if (response.contains("body") && !response["body"].is_null())
      return response["body"];
    if (response.contains("head") && !response["head"].is_null())
      return response["head"];
    return response;
  }

  // ─── Order Cancel ─────────────────────────

  json cancelOrder(const std::string &orderId) {
    json payload = genericPayload_;
    payload["body"]["ExchOrderID"] = orderId;
    json response = postJson(orderCancelRoute_, payload, true);
    std::cout << "response == " << response.dump(2) << std::endl;
    return response;
  }

  // ─── Order Modify ─────────────────────────

  json modifyOrder(const std::string &orderId, const std::string &price) {
    if (std::stod(price) <= 0)
      throw std::runtime_error("New price can't be 0.");

    json payload = genericPayload_;
    payload["body"]["ExchOrderID"] = orderId;
    payload["Price"] = price;
    json response = postJson(orderModifyRoute_, payload, true);
    std::cout << "response == " << response.dump(2) << std::endl;
    return response;
  }

  // ─── Trade Book ───────────────────────────

  json getTradebook() {
    json payload = genericPayload_;
    payload["body"]["ClientCode"] = clientCode_;
    json response = postJson(positionsRoute_, payload, true);
    std::cout << "response == " << response.dump(2) << std::endl;

    if (response.contains("body") && !response["body"].is_null() &&
        response["body"].contains("TradeBookDetail"))
      return response["body"]["TradeBookDetail"];
    return response;
  }

  // ─── Net-wise Positions ───────────────────

  json getNetwisePositions() {
    json payload = genericPayload_;
    payload["body"]["ClientCode"] = clientCode_;
    json response = postJson(netPositionRoute_, payload, true);
    std::cout << "response == " << response.dump(2) << std::endl;

    if (response.contains("body") && !response["body"].is_null() &&
        response["body"].contains("NetPositionDetail"))
      return response["body"]["NetPositionDetail"];
    return response;
  }

  // ─── Orders History ───────────────────────

  json getOrdersHistory() {
    json payload = genericPayload_;
    payload["body"]["ClientCode"] = clientCode_;
    json response = postJson(orderBookRoute_, payload, true);
    std::cout << "response == " << response.dump(2) << std::endl;

    if (response.contains("body") && !response["body"].is_null() &&
        response["body"].contains("OrderBookDetail"))
      return response["body"]["OrderBookDetail"];
    return response;
  }

  // ─── Single Order History ─────────────────

  json getOrderHistory(const std::string &orderId) {
    json payload = genericPayload_;
    payload["body"]["ClientCode"] = clientCode_;
    json response = postJson(orderBookRoute_, payload, true);
    std::cout << "response == " << response.dump(2) << std::endl;

    if (response.contains("body") && !response["body"].is_null() &&
        response["body"].contains("OrderBookDetail")) {
      for (auto &order : response["body"]["OrderBookDetail"]) {
        if (order.contains("BrokerOrderId") &&
            std::to_string(order["BrokerOrderId"].get<int64_t>()) == orderId) {
          return order;
        }
      }
      return json::object(); // not found
    }
    return response;
  }

  // ─── Account Balance ──────────────────────

  json getAccountBalance() {
    json payload = genericPayload_;
    payload["body"]["ClientCode"] = clientCode_;
    json response = postJson(multiOrderMarginRoute_, payload, false);

    if (response.contains("body") && !response["body"].is_null())
      return response["body"];
    return response;
  }

  // ─── Order Status ─────────────────────────

  json getOrderStatus(const std::string &orderId, const std::string &exch) {
    json payload = genericPayload_;
    payload["body"]["ClientCode"] = clientCode_;
    payload["body"]["OrdStatusReqList"] =
        json::array({{{"Exch", exch}, {"RemoteOrderID", orderId}}});

    json response = postJson(orderStatusRoute_, payload, true);
    std::cout << response.dump(2) << std::endl;

    if (response.contains("body") && !response["body"].is_null() &&
        response["body"].contains("OrdStatusResLst"))
      return response["body"]["OrdStatusResLst"];
    return response;
  }

  // ─── Holdings ─────────────────────────────

  json getHoldings() {
    json payload = genericPayload_;
    payload["body"]["ClientCode"] = clientCode_;
    json response = postJson(holdingsRoute_, payload, true);
    std::cout << "response == " << response.dump(2) << std::endl;

    if (response.contains("body") && !response["body"].is_null() &&
        response["body"].contains("Data"))
      return response["body"]["Data"];
    return response;
  }

  // ─── Getters ──────────────────────────────

  bool isAuthenticated() const { return authenticated_; }
  std::string getAccessToken() const { return accessToken_; }
  std::string getClientCode() const { return clientCode_; }

private:
  // Credentials
  std::string userId_;
  std::string userKey_;
  std::string appPassword_;
  std::string encryptionKey_;
  std::string clientCode_;
  std::string pin_;
  TOTPGenerator totp_;
  std::string appName_;

  // State
  bool authenticated_;
  std::string requestToken_;
  std::string accessToken_;

  // Payloads & headers
  json genericPayload_;
  std::map<std::string, std::string> headers_;

  // URLs
  std::string baseUrl_;
  std::string loginRoute_;
  std::string scripMasterRoute_;
  std::string orderBookRoute_;
  std::string holdingsRoute_;
  std::string positionsRoute_;
  std::string orderPlacementRoute_;
  std::string orderModifyRoute_;
  std::string orderCancelRoute_;
  std::string orderStatusRoute_;
  std::string tradeInfoRoute_;
  std::string jwtValidationRoute_;
  std::string historicalDataRoute_;
  std::string getRequestTokenRoute_;
  std::string accessTokenRoute_;
  std::string netPositionRoute_;
  std::string multiOrderMarginRoute_;

  // ─── HTTP Helper ──────────────────────────

  /**
   * POST JSON to a URL and return the parsed response.
   *
   * @param url              Target URL
   * @param payload          JSON body
   * @param includeAuth      Whether to include the Authorization header
   * @return parsed JSON response
   */
  json postJson(const std::string &url, const json &payload,
                bool includeAuth = false) {
    CURL *curl = curl_easy_init();
    if (!curl)
      throw std::runtime_error("Failed to initialize libcurl");

    std::string responseStr;
    std::string body = payload.dump();

    // Build header list
    struct curl_slist *curlHeaders = nullptr;
    for (auto &[key, value] : headers_) {
      if (!includeAuth && key == "Authorization")
        continue;
      std::string header = key + ": " + value;
      curlHeaders = curl_slist_append(curlHeaders, header.c_str());
    }

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, curlHeaders);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &responseStr);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER,
                     0L); // match Python verify=False
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);

    CURLcode res = curl_easy_perform(curl);

    curl_slist_free_all(curlHeaders);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
      throw std::runtime_error(std::string("HTTP request failed: ") +
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
//  main() – mirrors Python if __name__ == "__main__"
// ──────────────────────────────────────────────

// int main() {
//   // Initialize libcurl globally (once)
//   curl_global_init(CURL_GLOBAL_DEFAULT);
//   try {
//     FivepaisaBroker broker(
//         /* userId        */ "oQAzvA2GowP",
//         /* userKey       */ "nxNxc6UPyjbeiVUMDZFINmNFYcMyxm4X",
//         /* appPassword   */ "gLI00xNJJTn",
//         /* encryptionKey */ "6dd4Gs3HmuLqAR4N2j4nVnDbl7HpeExG",
//         /* clientCode    */ "56988153",
//         /* pin           */ "032002",
//         /* totpKey       */ "GU3DSOBYGE2TGXZVKBDUWRKZ",
//         /* appName       */ "ORDERMED");

//     broker.authenticate();

//     std::cout << "\n=== Net-wise Positions ===" << std::endl;
//     json positions = broker.getNetwisePositions();
//     std::cout << positions.dump(2) << std::endl;

//     std::cout << "\n=== Broker State ===" << std::endl;
//     std::cout << "  Authenticated: "
//               << (broker.isAuthenticated() ? "true" : "false") << std::endl;
//     std::cout << "  Client Code:   " << broker.getClientCode() << std::endl;
//     std::cout << "  Access Token:  " << broker.getAccessToken() << std::endl;

//    auto response = broker.placeOrder("35229", "N", "D", "B", 65, "NRML", "MARKET");    
//    std::cout << "response == " << response.dump(2) << std::endl;
  
// } catch (const std::exception &e) {
//     std::cerr << "Error: " << e.what() << std::endl;
//     curl_global_cleanup();
//     return 1;
//   }


//   curl_global_cleanup();
//   return 0;
// }
