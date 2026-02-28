#include <nlohmann/json.hpp>
#include <curl/curl.h>
#include <iostream>
#include <optional>
#include <string>

using json = nlohmann::json;
using namespace std;

class OrderAPI {
private:
  string baseUrl = "https://a3.aliceblueonline.com/";
  string authToken ="eyJhbGciOiJSUzI1NiIsInR5cCIgOiAiSldUIiwia2lkIiA6ICIyam9lOFVScGxZU3FTcDB3RDNVemVBQkgxYkpmOE4wSDRDMGVVSWhXUVAwIn0.eyJleHAiOjE3NzczNDYyMjIsImlhdCI6MTc3MjE2MjU4NywianRpIjoib25ydHJ0OjBlODA2OGUxLWVkZjgtMWQwNy03YzY0LTlkM2RmMGQyMGE3MCIsImlzcyI6Imh0dHBzOi8vaWRhYXMuYWxpY2VibHVlb25saW5lLmNvbS9pZGFhcy9yZWFsbXMvQWxpY2VCbHVlIiwiYXVkIjoiYWNjb3VudCIsInN1YiI6IjVlZjY2YmZiLTRjNWQtNDM5OS1iNmM2LTdjODViNjE0NjU2ZiIsInR5cCI6IkJlYXJlciIsImF6cCI6ImFsaWNlLWtiIiwic2lkIjoiMzlkMTU4OGQtZmMzMy1lN2EzLTUzZmMtNGIyM2JlYzMxZDdhIiwiYWxsb3dlZC1vcmlnaW5zIjpbImh0dHA6Ly9sb2NhbGhvc3Q6MzAwMiIsImh0dHA6Ly9sb2NhbGhvc3Q6NTA1MCIsImh0dHA6Ly9sb2NhbGhvc3Q6OTk0MyIsImh0dHA6Ly9sb2NhbGhvc3Q6OTAwMCJdLCJyZWFsbV9hY2Nlc3MiOnsicm9sZXMiOlsib2ZmbGluZV9hY2Nlc3MiLCJkZWZhdWx0LXJvbGVzLWFsaWNlYmx1ZWtiIiwidW1hX2F1dGhvcml6YXRpb24iXX0sInJlc291cmNlX2FjY2VzcyI6eyJhbGljZS1rYiI6eyJyb2xlcyI6WyJHVUVTVF9VU0VSIiwiQUNUSVZFX1VTRVIiXX0sImFjY291bnQiOnsicm9sZXMiOlsibWFuYWdlLWFjY291bnQiLCJtYW5hZ2UtYWNjb3VudC1saW5rcyIsInZpZXctcHJvZmlsZSJdfX0sInNjb3BlIjoiZW1haWwgcHJvZmlsZSBvcGVuaWQiLCJlbWFpbF92ZXJpZmllZCI6dHJ1ZSwidWNjIjoiMTgwNTY1NiIsImNsaWVudFJvbGUiOlsiR1VFU1RfVVNFUiIsIkFDVElWRV9VU0VSIl0sIm5hbWUiOiJKIFNhaSBLcmlzaG5hIiwicHJlZmVycmVkX3VzZXJuYW1lIjoiMTgwNTY1NiIsImdpdmVuX25hbWUiOiJKIFNhaSBLcmlzaG5hIn0.faUkt3kw_gdkwNSJMBSZvdHyQQDaXy7WkylwwXjvMDXfLj3FC_yjrUzraFg8PyGIFHABhS1vKcHGB8AND9pmNku_EIaHZ_f_CS6R5bGD1JXWOuvVEkg-jD6W_vPwOaEm4nJKSBHowppR3kSagHx2Q7Yzgsr1Wg9P8Ofaykky5engeiVaz9Qpjn_fvRj1J_sVk_xx23Jbg14jWL8c-pFw7YJLCn5PeR2kry7qUAiLNUNFgeH6VTCdFv6cbxkuEUamB2cyT6pYR7TfKVw8qE62uSi46OGykodh6cWcKh17QDdjcq0GJwl_0CaXBm8HgSh-ko4g314ZjCvnbN4mRScJLg"; // Load securely in
                                                        // production

  static size_t WriteCallback(void *contents, size_t size, size_t nmemb,
                              void *userp) {
    ((string *)userp)->append((char *)contents, size * nmemb);
    return size * nmemb;
  }

public:
  OrderAPI(string token) { authToken = token; }

  string placeOrder(string instrumentId, string exchange,
                    string transactionType, int quantity,
                    string orderComplexity, string product, string orderType,
                    string validity,

                    optional<double> price = nullopt,
                    optional<double> slTriggerPrice = nullopt,
                    optional<double> slLegPrice = nullopt,
                    optional<double> targetLegPrice = nullopt,
                    optional<double> trailingSlAmount = nullopt,
                    optional<int> disclosedQuantity = nullopt,
                    optional<double> marketProtectionPercent = nullopt,
                    optional<string> apiOrderSource = nullopt,
                    optional<string> algoId = nullopt,
                    optional<string> orderTag = nullopt) {

    // Build JSON
    json order = {{"instrumentId", instrumentId},
                  {"exchange", exchange},
                  {"transactionType", transactionType},
                  {"quantity", quantity},
                  {"orderComplexity", orderComplexity},
                  {"product", product},
                  {"orderType", orderType},
                  {"validity", validity}};

    // Optional values
    if (price)
      order["price"] = *price;
    if (slTriggerPrice)
      order["slTriggerPrice"] = *slTriggerPrice;
    if (slLegPrice)
      order["slLegPrice"] = *slLegPrice;
    if (targetLegPrice)
      order["targetLegPrice"] = *targetLegPrice;
    if (trailingSlAmount)
      order["trailingSlAmount"] = *trailingSlAmount;
    if (disclosedQuantity)
      order["disclosedQuantity"] = *disclosedQuantity;
    if (marketProtectionPercent)
      order["marketProtectionPercent"] = *marketProtectionPercent;
    if (apiOrderSource)
      order["apiOrderSource"] = *apiOrderSource;
    if (algoId)
      order["algoId"] = *algoId;
    if (orderTag)
      order["orderTag"] = *orderTag;

    json data = json::array();
    data.push_back(order);

    cout << "Sending JSON:\n" << data.dump(4) << endl;

    CURL *curl = curl_easy_init();
    CURLcode res;
    string response;

    if (curl) {

      string url = baseUrl + "open-api/od/v1/orders/placeorder";

      struct curl_slist *headers = NULL;

      string authHeader = "Authorization:" + authToken;
      headers = curl_slist_append(headers, authHeader.c_str());
      headers = curl_slist_append(headers, "Content-Type: application/json");

      curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
      curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
      curl_easy_setopt(curl, CURLOPT_POSTFIELDS, data.dump().c_str());
      curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
      curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

      res = curl_easy_perform(curl);

      if (res != CURLE_OK) {
        cerr << "Curl Error: " << curl_easy_strerror(res) << endl;
      }

      curl_slist_free_all(headers);
      curl_easy_cleanup(curl);
    }

    return response;
  }
};

int main() {

  // ⚠️ Put your actual session token here (or load from env)
  string token = "eyJhbGciOiJSUzI1NiIsInR5cCIgOiAiSldUIiwia2lkIiA6ICIyam9lOFVScGxZU3FTcDB3RDNVemVBQkgxYkpmOE4wSDRDMGVVSWhXUVAwIn0.eyJleHAiOjE3NzczNDYyMjIsImlhdCI6MTc3MjE2MjU4NywianRpIjoib25ydHJ0OjBlODA2OGUxLWVkZjgtMWQwNy03YzY0LTlkM2RmMGQyMGE3MCIsImlzcyI6Imh0dHBzOi8vaWRhYXMuYWxpY2VibHVlb25saW5lLmNvbS9pZGFhcy9yZWFsbXMvQWxpY2VCbHVlIiwiYXVkIjoiYWNjb3VudCIsInN1YiI6IjVlZjY2YmZiLTRjNWQtNDM5OS1iNmM2LTdjODViNjE0NjU2ZiIsInR5cCI6IkJlYXJlciIsImF6cCI6ImFsaWNlLWtiIiwic2lkIjoiMzlkMTU4OGQtZmMzMy1lN2EzLTUzZmMtNGIyM2JlYzMxZDdhIiwiYWxsb3dlZC1vcmlnaW5zIjpbImh0dHA6Ly9sb2NhbGhvc3Q6MzAwMiIsImh0dHA6Ly9sb2NhbGhvc3Q6NTA1MCIsImh0dHA6Ly9sb2NhbGhvc3Q6OTk0MyIsImh0dHA6Ly9sb2NhbGhvc3Q6OTAwMCJdLCJyZWFsbV9hY2Nlc3MiOnsicm9sZXMiOlsib2ZmbGluZV9hY2Nlc3MiLCJkZWZhdWx0LXJvbGVzLWFsaWNlYmx1ZWtiIiwidW1hX2F1dGhvcml6YXRpb24iXX0sInJlc291cmNlX2FjY2VzcyI6eyJhbGljZS1rYiI6eyJyb2xlcyI6WyJHVUVTVF9VU0VSIiwiQUNUSVZFX1VTRVIiXX0sImFjY291bnQiOnsicm9sZXMiOlsibWFuYWdlLWFjY291bnQiLCJtYW5hZ2UtYWNjb3VudC1saW5rcyIsInZpZXctcHJvZmlsZSJdfX0sInNjb3BlIjoiZW1haWwgcHJvZmlsZSBvcGVuaWQiLCJlbWFpbF92ZXJpZmllZCI6dHJ1ZSwidWNjIjoiMTgwNTY1NiIsImNsaWVudFJvbGUiOlsiR1VFU1RfVVNFUiIsIkFDVElWRV9VU0VSIl0sIm5hbWUiOiJKIFNhaSBLcmlzaG5hIiwicHJlZmVycmVkX3VzZXJuYW1lIjoiMTgwNTY1NiIsImdpdmVuX25hbWUiOiJKIFNhaSBLcmlzaG5hIn0.faUkt3kw_gdkwNSJMBSZvdHyQQDaXy7WkylwwXjvMDXfLj3FC_yjrUzraFg8PyGIFHABhS1vKcHGB8AND9pmNku_EIaHZ_f_CS6R5bGD1JXWOuvVEkg-jD6W_vPwOaEm4nJKSBHowppR3kSagHx2Q7Yzgsr1Wg9P8Ofaykky5engeiVaz9Qpjn_fvRj1J_sVk_xx23Jbg14jWL8c-pFw7YJLCn5PeR2kry7qUAiLNUNFgeH6VTCdFv6cbxkuEUamB2cyT6pYR7TfKVw8qE62uSi46OGykodh6cWcKh17QDdjcq0GJwl_0CaXBm8HgSh-ko4g314ZjCvnbN4mRScJLg";

  OrderAPI api(token);

  string response = api.placeOrder("35263",    // instrumentId
                                   "NFO",      // exchange
                                   "BUY",      // transactionType
                                   65,         // quantity
                                   "Regular",  // orderComplexity
                                   "LONGTERM", // product
                                   "MARKET",   // orderType
                                   "DAY",      // validity
                                   nullopt,    // price
                                   nullopt,    // slTriggerPrice
                                   nullopt,    // slLegPrice
                                   nullopt,    // targetLegPrice
                                   nullopt,    // trailingSlAmount
                                   nullopt,    // disclosedQuantity
                                   nullopt,    // marketProtectionPercent
                                   nullopt,    // apiOrderSource
                                   nullopt,    // algoId
                                   string("CPP trade") // orderTag
  );

  cout << "\nBroker Response:\n" << response << endl;

  return 0;
}