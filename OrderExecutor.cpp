/**
 * OrderExecutor.cpp — Universal Order Execution Handler (Implementation)
 *
 * Contains:
 *   • Mapping helpers     – translate universal args → broker-specific values
 *   • Broker dispatchers  – call each broker's placeOrder with mapped args
 *   • placeOrder()        – the single public entry point
 *
 * Compile (object-only, no link):
 *   g++ -std=c++17 -c OrderExecutor.cpp -I.
 *
 * To build a full executable you must also compile the broker .cpp files
 * and link them together.
 */

#include "OrderExecutor.h"

// ── Broker implementations
// Previously we included each broker .cpp directly which caused
// duplicate symbol errors for helpers like `WriteCallback` when all
// sources were merged into this translation unit. To keep the simple
// single-file workflow while avoiding symbol collisions, we redefine
// the `WriteCallback` symbol per-include using a macro.

#define WriteCallback WriteCallback_aliceblue
#include "aliceblue.cpp"
#undef WriteCallback

#define WriteCallback WriteCallback_angleone
#include "angleone.cpp"
#undef WriteCallback

#define WriteCallback WriteCallback_dhan
#include "dhan.cpp"
#undef WriteCallback

#define WriteCallback WriteCallback_findoc
#include "findoc.cpp"
#undef WriteCallback

#define WriteCallback WriteCallback_finvasia
#include "finvasia.cpp"
#undef WriteCallback

#define WriteCallback WriteCallback_fivepaisa
#include "fivepaisa.cpp"
#undef WriteCallback

#define WriteCallback WriteCallback_fyers
#include "fyers.cpp"
#undef WriteCallback

#define WriteCallback WriteCallback_kiteconnect
#include "kiteconnect.cpp"
#undef WriteCallback

#define WriteCallback WriteCallback_motilal
#include "motilal.cpp"
#undef WriteCallback

#define WriteCallback WriteCallback_upstox
#include "upstox.cpp"
#undef WriteCallback

// Forward declarations are already in OrderExecutor.h

#include <algorithm>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>

#ifdef _WIN32
#include <direct.h> // _mkdir
#else
#include <sys/stat.h> // mkdir
#endif

// ═════════════════════════════════════════════════════════════════════
//  String helpers
// ═════════════════════════════════════════════════════════════════════

std::string OrderExecutor::toLower(const std::string &s) {
  std::string r = s;
  std::transform(r.begin(), r.end(), r.begin(), ::tolower);
  return r;
}

std::string OrderExecutor::toUpper(const std::string &s) {
  std::string r = s;
  std::transform(r.begin(), r.end(), r.begin(), ::toupper);
  return r;
}

// ═════════════════════════════════════════════════════════════════════
//  Helper: is exchange a derivatives segment? (NFO, BFO, MCX)
// ═════════════════════════════════════════════════════════════════════

bool OrderExecutor::isDerivExchange(const std::string &exchange) {
  std::string u = toUpper(exchange);
  return (u == "NFO" || u == "BFO" || u == "MCX");
}

// ═════════════════════════════════════════════════════════════════════
//  Product mapping   (universal → broker-specific, EXCHANGE-DEPENDENT)
//
//  Key rule from the Python reference:
//    User passes "CNC" uniformly. On equity (NSE/BSE) it stays as the
//    equity-delivery product; on derivatives (NFO/BFO/MCX) it gets
//    translated to the broker's normal/carry-forward/margin product.
// ═════════════════════════════════════════════════════════════════════

// AliceBlue:  MIS→Intraday, CNC→Delivery, CNC(deriv)→Normal
// Python ref (L1267): prod_map = {"MIS":Intraday, "CNC":Delivery,
// "NRML":Normal}
std::string OrderExecutor::mapProductAliceBlue(const std::string &p,
                                               const std::string &exchange) {
  std::string u = toUpper(p);
  if (u == "INTRADAY" || u == "MIS")
    return "INTRADAY";
  if (u == "CNC") {
    // On NFO/BFO/MCX → NRML; on NSE/BSE → CNC (Python L1232)
    return isDerivExchange(exchange) ? "NRML" : "CNC";
  }
  if (u == "NRML" || u == "MARGIN")
    return "NRML";
  return "MIS";
}

// AngelOne:  MIS→INTRADAY, CNC→DELIVERY(equity)/CARRYFORWARD(deriv)
// Python ref (L1516): {"MIS":"INTRADAY", "CNC":"DELIVERY" if exch in [NSE,BSE]
// else "CARRYFORWARD"}
std::string OrderExecutor::mapProductAngelOne(const std::string &p,
                                              const std::string &exchange) {
  std::string u = toUpper(p);
  if (u == "INTRADAY" || u == "MIS")
    return "INTRADAY";
  if (u == "CNC") {
    return isDerivExchange(exchange) ? "CARRYFORWARD" : "DELIVERY";
  }
  if (u == "NRML")
    return "CARRYFORWARD";
  if (u == "MARGIN")
    return "MARGIN";
  return "INTRADAY";
}

// Dhan:  MIS→INTRADAY, CNC→CNC(equity)/MARGIN(deriv)
// Python ref (L1836): CNC → "MARGIN" if exchange in [MCX,NFO,BFO] else "CNC"
std::string OrderExecutor::mapProductDhan(const std::string &p,
                                          const std::string &exchange) {
  std::string u = toUpper(p);
  if (u == "INTRADAY" || u == "MIS")
    return "INTRADAY";
  if (u == "CNC") {
    return isDerivExchange(exchange) ? "MARGIN" : "CNC";
  }
  if (u == "NRML" || u == "MARGIN")
    return "MARGIN";
  return "INTRADAY";
}

// Fyers: MIS→INTRADAY, CNC→MARGIN (always, no equity CNC)
// Python ref (L1392): {"MIS":"INTRADAY", "CNC":"MARGIN"}
std::string OrderExecutor::mapProductFyers(const std::string &p,
                                           const std::string &) {
  std::string u = toUpper(p);
  if (u == "INTRADAY" || u == "MIS")
    return "INTRADAY";
  if (u == "CNC")
    return "MARGIN";
  if (u == "NRML" || u == "MARGIN")
    return "MARGIN";
  return "INTRADAY";
}

// Findoc: Python ref (L2048-2050) forces NRML always
std::string OrderExecutor::mapProductFindoc(const std::string &p,
                                            const std::string &exchange) {
  std::string u = toUpper(p);
  if (u == "INTRADAY" || u == "MIS")
    return "MIS";
  if (u == "CNC") {
    return isDerivExchange(exchange) ? "NRML" : "CNC";
  }
  if (u == "NRML" || u == "MARGIN")
    return "NRML";
  return "NRML"; // Python L2050: forced to NRML
}

// Finvasia: MIS→I, CNC→C(equity)/M(deriv)
// Python ref (L1622): {"MIS":"I", "CNC":"C" if exch in [NSE,BSE] else "M"}
std::string OrderExecutor::mapProductFinvasia(const std::string &p,
                                              const std::string &exchange) {
  std::string u = toUpper(p);
  if (u == "INTRADAY" || u == "MIS")
    return "I";
  if (u == "CNC") {
    return isDerivExchange(exchange) ? "M" : "C";
  }
  if (u == "NRML" || u == "MARGIN")
    return "M";
  return "I";
}

// 5paisa: MIS→MIS, CNC→NRML (always)
// Python ref (L1730): {"MIS":"MIS", "CNC":"NRML"}
std::string OrderExecutor::mapProductFivepaisa(const std::string &p,
                                               const std::string &) {
  std::string u = toUpper(p);
  if (u == "INTRADAY" || u == "MIS")
    return "MIS";
  if (u == "CNC" || u == "NRML")
    return "NRML";
  if (u == "MARGIN")
    return "NRML";
  return "MIS";
}

// KiteConnect (Zerodha): CNC→CNC(equity)/NRML(deriv)
// Python ref (L1003): if product=="CNC" and exchange in [MCX,NFO,BFO]:
// _product="NRML"
std::string OrderExecutor::mapProductKite(const std::string &p,
                                          const std::string &exchange) {
  std::string u = toUpper(p);
  if (u == "INTRADAY" || u == "MIS")
    return "MIS";
  if (u == "CNC") {
    return isDerivExchange(exchange) ? "NRML" : "CNC";
  }
  if (u == "NRML" || u == "MARGIN")
    return "NRML";
  return "MIS";
}

// Motilal Oswal: MIS→VALUEPLUS, CNC→DELIVERY(equity)/NORMAL(deriv)
// Python ref (L2151-2152): {"MIS":"VALUEPLUS", "CNC":"DELIVERY" if [NSE,BSE]
// else "NORMAL"}
std::string OrderExecutor::mapProductMotilal(const std::string &p,
                                             const std::string &exchange) {
  std::string u = toUpper(p);
  if (u == "INTRADAY" || u == "MIS")
    return "VALUEPLUS";
  if (u == "CNC") {
    return isDerivExchange(exchange) ? "NORMAL" : "DELIVERY";
  }
  if (u == "NRML" || u == "MARGIN")
    return "NORMAL";
  return "VALUEPLUS";
}

// Upstox: MIS→I, CNC→D (no exchange distinction)
// Python ref (L901-907): {"MIS":"I", "CNC":"D"}
std::string OrderExecutor::mapProductUpstox(const std::string &p,
                                            const std::string &) {
  std::string u = toUpper(p);
  if (u == "INTRADAY" || u == "MIS")
    return "I";
  if (u == "CNC")
    return "D";
  if (u == "NRML" || u == "MARGIN")
    return "D";
  return "I";
}

// ═════════════════════════════════════════════════════════════════════
//  OrderType mapping   (universal → broker-specific)
// ═════════════════════════════════════════════════════════════════════

std::string OrderExecutor::mapOrderTypeAngelOne(const std::string &ot) {
  std::string u = toUpper(ot);
  if (u == "MARKET")
    return "MARKET";
  if (u == "LIMIT")
    return "LIMIT";
  if (u == "SL")
    return "STOPLOSS_LIMIT";
  if (u == "SL-M")
    return "STOPLOSS_MARKET";
  return "MARKET";
}

std::string OrderExecutor::mapOrderTypeDhan(const std::string &ot) {
  std::string u = toUpper(ot);
  if (u == "MARKET")
    return "MARKET";
  if (u == "LIMIT")
    return "LIMIT";
  if (u == "SL")
    return "STOP_LOSS";
  if (u == "SL-M")
    return "STOP_LOSS_MARKET";
  return "MARKET";
}

int OrderExecutor::mapOrderTypeFyers(const std::string &ot) {
  std::string u = toUpper(ot);
  if (u == "LIMIT")
    return 1;
  if (u == "MARKET")
    return 2;
  if (u == "SL-M")
    return 3; // Stop order
  if (u == "SL")
    return 4; // Stop-Limit order
  return 2;   // default: MARKET
}

std::string OrderExecutor::mapOrderTypeFinvasia(const std::string &ot) {
  std::string u = toUpper(ot);
  if (u == "MARKET")
    return "MKT";
  if (u == "LIMIT")
    return "LMT";
  if (u == "SL")
    return "SL-LMT";
  if (u == "SL-M")
    return "SL-MKT";
  return "MKT";
}

std::string OrderExecutor::mapOrderTypeMotilal(const std::string &ot) {
  std::string u = toUpper(ot);
  if (u == "MARKET")
    return "MKT";
  if (u == "LIMIT")
    return "L";
  if (u == "SL")
    return "SL";
  if (u == "SL-M")
    return "SL-M";
  return "MKT";
}

// ═════════════════════════════════════════════════════════════════════
//  Exchange mapping   (universal → broker-specific)
// ═════════════════════════════════════════════════════════════════════

std::string OrderExecutor::mapExchangeDhan(const std::string &exch) {
  std::string u = toUpper(exch);
  if (u == "NSE")
    return "NSE_EQ";
  if (u == "BSE")
    return "BSE_EQ";
  if (u == "NFO")
    return "NSE_FNO";
  if (u == "BFO")
    return "BSE_FNO";
  if (u == "MCX")
    return "MCX_COMM";
  return u;
}

std::string OrderExecutor::mapExchangeFindoc(const std::string &exch) {
  std::string u = toUpper(exch);
  if (u == "NSE")
    return "NSECM";
  if (u == "BSE")
    return "BSECM";
  if (u == "NFO")
    return "NSEFO";
  if (u == "BFO")
    return "BSEFO";
  if (u == "MCX")
    return "MCXFO";
  return u;
}

std::string OrderExecutor::mapExchangeFivepaisa(const std::string &exch) {
  std::string u = toUpper(exch);
  if (u == "NSE" || u == "NFO")
    return "N";
  if (u == "BSE" || u == "BFO")
    return "B";
  if (u == "MCX")
    return "M";
  return "N";
}

std::string OrderExecutor::mapExchangeTypeFivepaisa(const std::string &exch) {
  std::string u = toUpper(exch);
  if (u == "NSE" || u == "BSE")
    return "C"; // Cash
  if (u == "NFO" || u == "BFO" || u == "MCX")
    return "D"; // Derivatives
  return "C";
}

// ═════════════════════════════════════════════════════════════════════
//  Side / Transaction-type mapping
// ═════════════════════════════════════════════════════════════════════

int OrderExecutor::mapSideFyers(const std::string &side) {
  return (toUpper(side) == "BUY") ? 1 : -1;
}

std::string OrderExecutor::mapSideFinvasia(const std::string &side) {
  return (toUpper(side) == "BUY") ? "B" : "S";
}

std::string OrderExecutor::mapSideFivepaisa(const std::string &side) {
  return (toUpper(side) == "BUY") ? "B" : "S";
}

std::string OrderExecutor::mapSideMotilal(const std::string &side) {
  return (toUpper(side) == "BUY") ? "B" : "S";
}

// ═════════════════════════════════════════════════════════════════════
//  Constructor
// ═════════════════════════════════════════════════════════════════════

OrderExecutor::OrderExecutor() {
  std::cout << "OrderExecutor initialized." << std::endl;
}

// ═════════════════════════════════════════════════════════════════════
//  Registration
// ═════════════════════════════════════════════════════════════════════

void OrderExecutor::registerBroker(const std::string &userId,
                                   const std::string &brokerName,
                                   void *brokerPtr) {
  BrokerInfo info;
  info.broker_name = toLower(brokerName);
  info.broker_ptr = brokerPtr;
  registry_[userId] = info;
  std::cout << "Registered user [" << userId << "] with broker ["
            << info.broker_name << "]" << std::endl;
}

// ═════════════════════════════════════════════════════════════════════
//  Utility
// ═════════════════════════════════════════════════════════════════════

std::vector<std::pair<std::string, std::string>>
OrderExecutor::listRegistered() const {
  std::vector<std::pair<std::string, std::string>> out;
  for (auto &[uid, info] : registry_)
    out.emplace_back(uid, info.broker_name);
  return out;
}

bool OrderExecutor::isRegistered(const std::string &userId) const {
  return registry_.count(userId) > 0;
}

// ═════════════════════════════════════════════════════════════════════
//  Main dispatcher
// ═════════════════════════════════════════════════════════════════════

OrderResult OrderExecutor::placeOrder(const std::string &userId,
                                      const UniversalOrder &order) {
  OrderResult result;
  result.user_id = userId;

  auto it = registry_.find(userId);
  if (it == registry_.end()) {
    result.message = "User [" + userId + "] is not registered.";
    std::cerr << "[OrderExecutor] " << result.message << std::endl;
    return result;
  }

  const BrokerInfo &info = it->second;
  result.broker = info.broker_name;

  try {
    if (info.broker_name == "aliceblue") {
      result = placeOrderAliceBlue(
          static_cast<AliceBlueBroker *>(info.broker_ptr), userId, order);
    } else if (info.broker_name == "angelone") {
      result = placeOrderAngelOne(
          static_cast<AngelOneBroker *>(info.broker_ptr), userId, order);
    } else if (info.broker_name == "dhan") {
      result = placeOrderDhan(static_cast<DhanBroker *>(info.broker_ptr),
                              userId, order);
    } else if (info.broker_name == "fyers") {
      result = placeOrderFyers(static_cast<FyersBroker *>(info.broker_ptr),
                               userId, order);
    } else if (info.broker_name == "findoc") {
      result = placeOrderFindoc(static_cast<FindocBroker *>(info.broker_ptr),
                                userId, order);
    } else if (info.broker_name == "finvasia") {
      result = placeOrderFinvasia(
          static_cast<FinvasiaBroker *>(info.broker_ptr), userId, order);
    } else if (info.broker_name == "5paisa") {
      result = placeOrderFivepaisa(
          static_cast<FivepaisaBroker *>(info.broker_ptr), userId, order);
    } else if (info.broker_name == "kiteconnect") {
      result = placeOrderKiteConnect(
          static_cast<KiteConnect *>(info.broker_ptr), userId, order);
    } else if (info.broker_name == "motilal") {
      result = placeOrderMotilal(
          static_cast<MotilalOswalBroker *>(info.broker_ptr), userId, order);
    } else if (info.broker_name == "upstox") {
      result = placeOrderUpstox(static_cast<UpstoxBroker *>(info.broker_ptr),
                                userId, order);
    } else {
      result.message = "Unknown broker: " + info.broker_name;
      std::cerr << "[OrderExecutor] " << result.message << std::endl;
    }
  } catch (const std::exception &ex) {
    result.success = false;
    result.message = std::string("Exception: ") + ex.what();
    std::cerr << "[OrderExecutor] " << result.message << std::endl;
  }

  return result;
}

// ═════════════════════════════════════════════════════════════════════
//  AliceBlue dispatcher
// ═════════════════════════════════════════════════════════════════════

OrderResult OrderExecutor::placeOrderAliceBlue(AliceBlueBroker *b,
                                               const std::string &userId,
                                               const UniversalOrder &o) {
  OrderResult res;
  res.user_id = userId;
  res.broker = "aliceblue";

  std::string product = mapProductAliceBlue(o.product, o.exchange);
  std::string orderType = toUpper(o.order_type); // MARKET/LIMIT/SL/SL-M
  std::string price = (o.price > 0) ? std::to_string(o.price) : "0";
  std::string trigPrice =
      (o.trigger_price > 0) ? std::to_string(o.trigger_price) : "";
  std::string complexity = "REGULAR";
  if (toUpper(o.variety) == "BO")
    complexity = "BO";

  json resp = b->placeOrder(
      o.exchange, o.symbol_token, toUpper(o.transaction_type), o.quantity,
      product, complexity, orderType, o.validity, price, trigPrice,
      /*slLegPrice*/ "", /*targetLegPrice*/ "", std::to_string(o.disclosed_qty),
      /*marketProtPct*/ "",
      /*trailingSlAmt*/ "", o.tag);

  res.raw_response = resp;
  res.success = !resp.is_null();
  res.message = resp.dump();

  // Try to extract order ID from response
  if (resp.is_array() && !resp.empty() && resp[0].contains("orderNumber")) {
    res.order_id = resp[0]["orderNumber"].get<std::string>();
  }

  return res;
}

// ═════════════════════════════════════════════════════════════════════
//  AngelOne dispatcher
// ═════════════════════════════════════════════════════════════════════

OrderResult OrderExecutor::placeOrderAngelOne(AngelOneBroker *b,
                                              const std::string &userId,
                                              const UniversalOrder &o) {
  OrderResult res;
  res.user_id = userId;
  res.broker = "angelone";

  std::string variety = "NORMAL";
  std::string v = toUpper(o.variety);
  if (v == "AMO")
    variety = "AMO";
  else if (v == "BO")
    variety = "ROBO";
  else if (v == "SL" || toUpper(o.order_type) == "SL" ||
           toUpper(o.order_type) == "SL-M")
    variety = "STOPLOSS";

  json orderParams = {
      {"variety", variety},
      {"tradingsymbol", o.trading_symbol},
      {"symboltoken", o.symbol_token},
      {"transactiontype", toUpper(o.transaction_type)},
      {"exchange", toUpper(o.exchange)},
      {"ordertype", mapOrderTypeAngelOne(o.order_type)},
      {"producttype", mapProductAngelOne(o.product, o.exchange)},
      {"duration", o.validity.empty() ? "DAY" : toUpper(o.validity)},
      {"price", std::to_string(o.price)},
      {"quantity", std::to_string(o.quantity)},
      {"triggerprice", std::to_string(o.trigger_price)}};

  if (!o.tag.empty())
    orderParams["ordertag"] = o.tag;

  json resp = b->placeOrder(orderParams);

  res.raw_response = resp;
  res.success = resp.contains("data") && !resp["data"].is_null();
  res.message = resp.dump();

  if (res.success && resp["data"].contains("orderid"))
    res.order_id = resp["data"]["orderid"].get<std::string>();

  return res;
}

// ═════════════════════════════════════════════════════════════════════
//  Dhan dispatcher
// ═════════════════════════════════════════════════════════════════════

OrderResult OrderExecutor::placeOrderDhan(DhanBroker *b,
                                          const std::string &userId,
                                          const UniversalOrder &o) {
  OrderResult res;
  res.user_id = userId;
  res.broker = "dhan";

  json resp = b->placeOrder(
      o.symbol_token, mapExchangeDhan(o.exchange), toUpper(o.transaction_type),
      o.quantity, mapOrderTypeDhan(o.order_type),
      mapProductDhan(o.product, o.exchange), o.price, o.trigger_price,
      o.disclosed_qty, o.is_amo,
      o.validity.empty() ? "DAY" : toUpper(o.validity),
      /*amoTime*/ "OPEN", /*boProfitValue*/ 0, /*boStopLossValue*/ 0, o.tag);

  res.raw_response = resp;
  res.success =
      resp.contains("status") && resp["status"].get<std::string>() == "success";
  res.message = resp.dump();

  if (res.success && resp.contains("data") && resp["data"].contains("orderId"))
    res.order_id = resp["data"]["orderId"].get<std::string>();

  return res;
}

// ═════════════════════════════════════════════════════════════════════
//  Fyers dispatcher
// ═════════════════════════════════════════════════════════════════════

OrderResult OrderExecutor::placeOrderFyers(FyersBroker *b,
                                           const std::string &userId,
                                           const UniversalOrder &o) {
  OrderResult res;
  res.user_id = userId;
  res.broker = "fyers";

  // Fyers symbol format: "NSE:SBIN-EQ"
  std::string symbol = toUpper(o.exchange) + ":" + o.trading_symbol;

  json resp = b->placeOrder(
      symbol, o.quantity, mapOrderTypeFyers(o.order_type),
      mapSideFyers(o.transaction_type), mapProductFyers(o.product, o.exchange),
      o.price, o.trigger_price,
      o.validity.empty() ? "DAY" : toUpper(o.validity), o.disclosed_qty,
      /*offlineOrder*/ o.is_amo);

  res.raw_response = resp;
  res.success = resp.contains("s") && resp["s"].get<std::string>() == "ok";
  res.message = resp.dump();

  if (resp.contains("id"))
    res.order_id = resp["id"].get<std::string>();

  return res;
}

// ═════════════════════════════════════════════════════════════════════
//  Findoc dispatcher
// ═════════════════════════════════════════════════════════════════════

OrderResult OrderExecutor::placeOrderFindoc(FindocBroker *b,
                                            const std::string &userId,
                                            const UniversalOrder &o) {
  OrderResult res;
  res.user_id = userId;
  res.broker = "findoc";

  int exchangeToken = 0;
  try {
    exchangeToken = std::stoi(o.symbol_token);
  } catch (...) {
    res.message = "Invalid symbol_token for Findoc (must be integer)";
    return res;
  }

  std::string orderType = toUpper(o.order_type);
  // Findoc only supports LIMIT and MARKET
  if (orderType != "LIMIT")
    orderType = "MARKET";

  json resp = b->placeOrder(
      mapExchangeFindoc(o.exchange), exchangeToken, toUpper(o.transaction_type),
      mapProductFindoc(o.product, o.exchange), orderType, o.quantity, o.price,
      o.trigger_price, o.disclosed_qty,
      o.validity.empty() ? "DAY" : toUpper(o.validity), o.tag);

  res.raw_response = resp;
  res.success = !resp.is_null();
  res.message = resp.dump();

  if (resp.contains("AppOrderID"))
    res.order_id = std::to_string(resp["AppOrderID"].get<int>());

  return res;
}

// ═════════════════════════════════════════════════════════════════════
//  Finvasia (Shoonya) dispatcher
// ═════════════════════════════════════════════════════════════════════

OrderResult OrderExecutor::placeOrderFinvasia(FinvasiaBroker *b,
                                              const std::string &userId,
                                              const UniversalOrder &o) {
  OrderResult res;
  res.user_id = userId;
  res.broker = "finvasia";

  json resp = b->placeOrder(
      mapSideFinvasia(o.transaction_type),
      mapProductFinvasia(o.product, o.exchange), toUpper(o.exchange),
      o.trading_symbol, o.quantity, mapOrderTypeFinvasia(o.order_type), o.price,
      o.trigger_price, o.disclosed_qty,
      o.validity.empty() ? "DAY" : toUpper(o.validity), o.tag);

  res.raw_response = resp;
  res.success =
      resp.contains("stat") && resp["stat"].get<std::string>() == "Ok";
  res.message = resp.dump();

  if (resp.contains("norenordno"))
    res.order_id = resp["norenordno"].get<std::string>();

  return res;
}

// ═════════════════════════════════════════════════════════════════════
//  5paisa dispatcher
// ═════════════════════════════════════════════════════════════════════

OrderResult OrderExecutor::placeOrderFivepaisa(FivepaisaBroker *b,
                                               const std::string &userId,
                                               const UniversalOrder &o) {
  OrderResult res;
  res.user_id = userId;
  res.broker = "5paisa";

  std::string exchType = o.exchange_type.empty()
                             ? mapExchangeTypeFivepaisa(o.exchange)
                             : o.exchange_type;

  json resp =
      b->placeOrder(o.symbol_token, mapExchangeFivepaisa(o.exchange), exchType,
                    mapSideFivepaisa(o.transaction_type), o.quantity,
                    mapProductFivepaisa(o.product, o.exchange),
                    toUpper(o.order_type) == "LIMIT" ? "LIMIT" : "MARKET",
                    o.tag, o.price, o.disclosed_qty, o.trigger_price);

  res.raw_response = resp;
  res.success = !resp.is_null();
  res.message = resp.dump();

  if (resp.contains("BrokerOrderID"))
    res.order_id = std::to_string(resp["BrokerOrderID"].get<int64_t>());

  return res;
}

// ═════════════════════════════════════════════════════════════════════
//  KiteConnect (Zerodha) dispatcher
// ═════════════════════════════════════════════════════════════════════

OrderResult OrderExecutor::placeOrderKiteConnect(KiteConnect *b,
                                                 const std::string &userId,
                                                 const UniversalOrder &o) {
  OrderResult res;
  res.user_id = userId;
  res.broker = "kiteconnect";

  std::string variety = toLower(o.variety);
  if (variety.empty() || variety == "regular")
    variety = "regular";
  else if (variety == "amo")
    variety = "amo";
  else if (variety == "co")
    variety = "co";
  else if (variety == "bo")
    variety = "iceberg";

  std::string priceStr = (o.price > 0) ? std::to_string(o.price) : "";
  std::string trigStr =
      (o.trigger_price > 0) ? std::to_string(o.trigger_price) : "";
  std::string dqStr =
      (o.disclosed_qty > 0) ? std::to_string(o.disclosed_qty) : "";

  std::string orderId = b->placeOrder(
      variety, toUpper(o.exchange), o.trading_symbol,
      toUpper(o.transaction_type), o.quantity,
      mapProductKite(o.product, o.exchange), toUpper(o.order_type), priceStr,
      o.validity.empty() ? "" : toUpper(o.validity), trigStr, dqStr, o.tag);

  res.order_id = orderId;
  res.success = !orderId.empty();
  res.message = orderId.empty() ? "No order_id returned" : orderId;

  return res;
}

// ═════════════════════════════════════════════════════════════════════
//  Motilal Oswal dispatcher
// ═════════════════════════════════════════════════════════════════════

OrderResult OrderExecutor::placeOrderMotilal(MotilalOswalBroker *b,
                                             const std::string &userId,
                                             const UniversalOrder &o) {
  OrderResult res;
  res.user_id = userId;
  res.broker = "motilal";

  json orderInfo = {
      {"exchange", toUpper(o.exchange)},
      {"symboltoken", o.symbol_token},
      {"buyorsell", mapSideMotilal(o.transaction_type)},
      {"ordertype", mapOrderTypeMotilal(o.order_type)},
      {"producttype", mapProductMotilal(o.product, o.exchange)},
      {"orderduration", o.validity.empty() ? "DAY" : toUpper(o.validity)},
      {"price", std::to_string(o.price)},
      {"triggerprice", std::to_string(o.trigger_price)},
      {"quantity", std::to_string(o.quantity)},
      {"disclosedquantity", std::to_string(o.disclosed_qty)},
      {"amoorder", o.is_amo ? "Y" : "N"}};

  if (!o.tag.empty())
    orderInfo["tag"] = o.tag;

  json resp = b->placeOrder(orderInfo);

  res.raw_response = resp;
  res.success = !resp.is_null();
  res.message = resp.dump();

  if (resp.contains("uniqueorderid"))
    res.order_id = resp["uniqueorderid"].get<std::string>();

  return res;
}

// ═════════════════════════════════════════════════════════════════════
//  Upstox dispatcher
// ═════════════════════════════════════════════════════════════════════

OrderResult OrderExecutor::placeOrderUpstox(UpstoxBroker *b,
                                            const std::string &userId,
                                            const UniversalOrder &o) {
  OrderResult res;
  res.user_id = userId;
  res.broker = "upstox";

  // Upstox expects instrument_token like "NSE_EQ|INE669E01016"
  // The user can either pass the full token or we use symbol_token as-is
  std::string instrumentToken = o.symbol_token;

  json resp = b->placeOrder(
      instrumentToken, o.quantity, mapProductUpstox(o.product, o.exchange),
      o.validity.empty() ? "DAY" : toUpper(o.validity), toUpper(o.order_type),
      toUpper(o.transaction_type), o.price, o.trigger_price, o.disclosed_qty,
      o.is_amo, o.tag);

  res.raw_response = resp;
  res.success = !resp.is_null();
  res.message = resp.dump();

  if (resp.contains("order_id"))
    res.order_id = resp["order_id"].get<std::string>();

  return res;
}

// ═════════════════════════════════════════════════════════════════════
//  InstrumentNormalizer — URL constants (matching Python reference)
// ═════════════════════════════════════════════════════════════════════

const std::map<std::string, std::string> InstrumentNormalizer::ALICE_URLS = {
    {"NSE",
     "https://v2api.aliceblueonline.com/restpy/static/contract_master/NSE.csv"},
    {"BSE",
     "https://v2api.aliceblueonline.com/restpy/static/contract_master/BSE.csv"},
    {"NFO",
     "https://v2api.aliceblueonline.com/restpy/static/contract_master/NFO.csv"},
    {"BFO",
     "https://v2api.aliceblueonline.com/restpy/static/contract_master/BFO.csv"},
    {"MCX",
     "https://v2api.aliceblueonline.com/restpy/static/contract_master/MCX.csv"},
};

const std::map<std::string, std::string> InstrumentNormalizer::FYERS_URLS = {
    {"NSE", "https://public.fyers.in/sym_details/NSE_CM.csv"},
    {"BSE", "https://public.fyers.in/sym_details/BSE_CM.csv"},
    {"NFO", "https://public.fyers.in/sym_details/NSE_FO.csv"},
    {"BFO", "https://public.fyers.in/sym_details/BSE_FO.csv"},
    {"MCX", "https://public.fyers.in/sym_details/MCX_COM.csv"},
};

const std::string InstrumentNormalizer::ANGEL_URL =
    "https://margincalculator.angelbroking.com/OpenAPI_File/files/"
    "OpenAPIScripMaster.json";

const std::string InstrumentNormalizer::ZERODHA_URL =
    "https://api.kite.trade/instruments";

const std::string InstrumentNormalizer::UPSTOX_URL =
    "https://assets.upstox.com/market-quote/instruments/exchange/"
    "complete.csv.gz";

const std::map<std::string, std::string>
    InstrumentNormalizer::UPSTOX_EXCHANGE_MAP = {
        {"NFO", "NSE_FO"}, {"BFO", "BSE_FO"}, {"MCX", "MCX_FO"},
        {"BSE", "BSE_EQ"}, {"NSE", "NSE_EQ"},
};

// ═════════════════════════════════════════════════════════════════════
//  InstrumentNormalizer — Helper utilities
// ═════════════════════════════════════════════════════════════════════

// libcurl write callback for downloading
static size_t NormalizerWriteCB(void *contents, size_t size, size_t nmemb,
                                void *userp) {
  ((std::string *)userp)->append((char *)contents, size * nmemb);
  return size * nmemb;
}

std::string InstrumentNormalizer::downloadUrl(const std::string &url) {
  std::string response;
  CURL *curl = curl_easy_init();
  if (!curl)
    throw std::runtime_error("Failed to init curl");

  curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, NormalizerWriteCB);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
  curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
  curl_easy_setopt(curl, CURLOPT_TIMEOUT, 120L);
  curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
  // For gzipped content (Upstox)
  curl_easy_setopt(curl, CURLOPT_ACCEPT_ENCODING, "");

  CURLcode res = curl_easy_perform(curl);
  curl_easy_cleanup(curl);

  if (res != CURLE_OK)
    throw std::runtime_error("Download failed: " +
                             std::string(curl_easy_strerror(res)));

  return response;
}

std::vector<std::vector<std::string>>
InstrumentNormalizer::parseCSV(const std::string &data) {
  std::vector<std::vector<std::string>> rows;
  std::istringstream stream(data);
  std::string line;

  while (std::getline(stream, line)) {
    // Strip \r
    if (!line.empty() && line.back() == '\r')
      line.pop_back();
    if (line.empty())
      continue;

    std::vector<std::string> row;
    std::string field;
    bool inQuotes = false;

    for (size_t i = 0; i < line.size(); ++i) {
      char c = line[i];
      if (c == '"') {
        inQuotes = !inQuotes;
      } else if (c == ',' && !inQuotes) {
        row.push_back(field);
        field.clear();
      } else {
        field += c;
      }
    }
    row.push_back(field);
    rows.push_back(std::move(row));
  }
  return rows;
}

int InstrumentNormalizer::findColumn(const std::vector<std::string> &header,
                                     const std::string &name) {
  for (int i = 0; i < (int)header.size(); ++i) {
    // Trim whitespace for comparison
    std::string h = header[i];
    while (!h.empty() && (h.front() == ' ' || h.front() == '\t'))
      h.erase(h.begin());
    while (!h.empty() && (h.back() == ' ' || h.back() == '\t'))
      h.pop_back();
    if (h == name)
      return i;
  }
  return -1;
}

bool InstrumentNormalizer::fileExists(const std::string &path) {
  std::ifstream f(path);
  return f.good();
}

void InstrumentNormalizer::writeFile(const std::string &path,
                                     const std::string &data) {
  std::ofstream f(path, std::ios::binary);
  f.write(data.c_str(), data.size());
}

std::string InstrumentNormalizer::readFile(const std::string &path) {
  std::ifstream f(path, std::ios::binary);
  return std::string((std::istreambuf_iterator<char>(f)),
                     std::istreambuf_iterator<char>());
}

// ═════════════════════════════════════════════════════════════════════
//  InstrumentNormalizer — Master loaders
// ═════════════════════════════════════════════════════════════════════

void InstrumentNormalizer::loadAliceMaster(const std::string &dir, bool force) {
  for (auto &[exchange, url] : ALICE_URLS) {
    std::string cachePath = dir + "/alice_" + exchange + ".csv";
    std::string csvData;

    if (force || !fileExists(cachePath)) {
      std::cout << "[InstrumentNormalizer] Downloading Alice " << exchange
                << " master..." << std::endl;
      csvData = downloadUrl(url);
      writeFile(cachePath, csvData);
    } else {
      std::cout << "[InstrumentNormalizer] Loading Alice " << exchange
                << " from cache..." << std::endl;
      csvData = readFile(cachePath);
    }

    auto rows = parseCSV(csvData);
    if (rows.size() < 2)
      continue;

    auto &header = rows[0];
    int iToken = findColumn(header, "Token");
    int iTradSym = findColumn(header, "Trading Symbol");
    int iSymbol = findColumn(header, "Symbol");
    int iLotSize = findColumn(header, "Lot Size");
    int iTickSize = findColumn(header, "Tick Size");

    if (iToken < 0 || iTradSym < 0) {
      std::cerr << "[InstrumentNormalizer] Alice " << exchange
                << ": missing Token/Trading Symbol columns" << std::endl;
      continue;
    }

    auto &exchMap = masters_["alice"][exchange];
    for (size_t r = 1; r < rows.size(); ++r) {
      auto &row = rows[r];
      if (row.size() <= (size_t)std::max({iToken, iTradSym, iSymbol, iLotSize}))
        continue;

      InstrumentInfo info;
      try {
        info.token = std::stoi(row[iToken]);
      } catch (...) {
        continue;
      }

      info.trading_symbol = row[iTradSym];
      if (iSymbol >= 0 && (size_t)iSymbol < row.size())
        info.symbol = row[iSymbol];
      if (iLotSize >= 0 && (size_t)iLotSize < row.size()) {
        try {
          info.lot_size = std::stoi(row[iLotSize]);
        } catch (...) {
        }
      }
      if (iTickSize >= 0 && (size_t)iTickSize < row.size()) {
        try {
          info.tick_size = std::stod(row[iTickSize]);
        } catch (...) {
        }
      }

      exchMap[info.token] = std::move(info);
    }
    std::cout << "[InstrumentNormalizer] Alice " << exchange << ": "
              << exchMap.size() << " instruments loaded" << std::endl;
  }
}

void InstrumentNormalizer::loadAngelMaster(const std::string &dir, bool force) {
  std::string cachePath = dir + "/angel.json";
  std::string jsonData;

  if (force || !fileExists(cachePath)) {
    std::cout << "[InstrumentNormalizer] Downloading Angel master..."
              << std::endl;
    jsonData = downloadUrl(ANGEL_URL);
    writeFile(cachePath, jsonData);
  } else {
    std::cout << "[InstrumentNormalizer] Loading Angel from cache..."
              << std::endl;
    jsonData = readFile(cachePath);
  }

  try {
    json instruments = json::parse(jsonData);
    for (auto &item : instruments) {
      std::string exchSeg = item.value("exch_seg", "");
      if (exchSeg != "NSE" && exchSeg != "BSE" && exchSeg != "NFO" &&
          exchSeg != "BFO" && exchSeg != "MCX")
        continue;

      InstrumentInfo info;
      try {
        std::string tokenStr = item.value("token", "0");
        info.token = std::stoi(tokenStr);
      } catch (...) {
        continue;
      }

      info.trading_symbol = item.value("symbol", "");
      info.symbol = item.value("name", "");
      try {
        std::string lotStr = item.value("lotsize", "1");
        info.lot_size = std::stoi(lotStr);
      } catch (...) {
      }

      masters_["angel"][exchSeg][info.token] = std::move(info);
    }

    for (auto &[exch, m] : masters_["angel"]) {
      std::cout << "[InstrumentNormalizer] Angel " << exch << ": " << m.size()
                << " instruments loaded" << std::endl;
    }
  } catch (const std::exception &e) {
    std::cerr << "[InstrumentNormalizer] Angel parse error: " << e.what()
              << std::endl;
  }
}

void InstrumentNormalizer::loadZerodhaMaster(const std::string &dir,
                                             bool force) {
  std::string cachePath = dir + "/zerodha.csv";
  std::string csvData;

  if (force || !fileExists(cachePath)) {
    std::cout << "[InstrumentNormalizer] Downloading Zerodha master..."
              << std::endl;
    csvData = downloadUrl(ZERODHA_URL);
    writeFile(cachePath, csvData);
  } else {
    std::cout << "[InstrumentNormalizer] Loading Zerodha from cache..."
              << std::endl;
    csvData = readFile(cachePath);
  }

  auto rows = parseCSV(csvData);
  if (rows.size() < 2)
    return;

  auto &header = rows[0];
  int iExchToken = findColumn(header, "exchange_token");
  int iInstToken = findColumn(header, "instrument_token");
  int iTradSym = findColumn(header, "tradingsymbol");
  int iExchange = findColumn(header, "exchange");
  int iLotSize = findColumn(header, "lot_size");
  int iTickSize = findColumn(header, "tick_size");
  int iName = findColumn(header, "name");
  int iSegment = findColumn(header, "segment");

  if (iExchToken < 0 || iTradSym < 0 || iExchange < 0)
    return;

  for (size_t r = 1; r < rows.size(); ++r) {
    auto &row = rows[r];
    if (row.size() <= (size_t)std::max({iExchToken, iTradSym, iExchange}))
      continue;

    // Filter to relevant segments
    if (iSegment >= 0 && (size_t)iSegment < row.size()) {
      std::string seg = row[iSegment];
      if (seg.find("NFO") == std::string::npos &&
          seg.find("BFO") == std::string::npos &&
          seg.find("MCX") == std::string::npos && seg != "NSE" && seg != "BSE")
        continue;
    }

    InstrumentInfo info;
    try {
      info.token = std::stoi(row[iExchToken]);
    } catch (...) {
      continue;
    }
    info.trading_symbol = row[iTradSym];

    if (iInstToken >= 0 && (size_t)iInstToken < row.size()) {
      try {
        info.instrument_token = std::stoi(row[iInstToken]);
      } catch (...) {
      }
    }
    if (iLotSize >= 0 && (size_t)iLotSize < row.size()) {
      try {
        info.lot_size = std::stoi(row[iLotSize]);
      } catch (...) {
      }
    }
    if (iTickSize >= 0 && (size_t)iTickSize < row.size()) {
      try {
        info.tick_size = std::stod(row[iTickSize]);
      } catch (...) {
      }
    }
    if (iName >= 0 && (size_t)iName < row.size())
      info.symbol = row[iName];

    std::string exch = row[iExchange];
    masters_["zerodha"][exch][info.token] = std::move(info);
  }

  for (auto &[exch, m] : masters_["zerodha"]) {
    std::cout << "[InstrumentNormalizer] Zerodha " << exch << ": " << m.size()
              << " instruments loaded" << std::endl;
  }
}

void InstrumentNormalizer::loadUpstoxMaster(const std::string &dir,
                                            bool force) {
  std::string cachePath = dir + "/upstox.csv";
  std::string csvData;

  if (force || !fileExists(cachePath)) {
    std::cout << "[InstrumentNormalizer] Downloading Upstox master..."
              << std::endl;
    csvData = downloadUrl(UPSTOX_URL);
    writeFile(cachePath, csvData);
  } else {
    std::cout << "[InstrumentNormalizer] Loading Upstox from cache..."
              << std::endl;
    csvData = readFile(cachePath);
  }

  auto rows = parseCSV(csvData);
  if (rows.size() < 2)
    return;

  auto &header = rows[0];
  int iExchToken = findColumn(header, "exchange_token");
  int iInstKey = findColumn(header, "instrument_key");
  int iTradSym = findColumn(header, "tradingsymbol");
  int iExchange = findColumn(header, "exchange");
  int iLotSize = findColumn(header, "lot_size");
  int iTickSize = findColumn(header, "tick_size");
  int iName = findColumn(header, "name");

  if (iExchToken < 0 || iExchange < 0)
    return;

  for (size_t r = 1; r < rows.size(); ++r) {
    auto &row = rows[r];
    if (row.size() <= (size_t)std::max(iExchToken, iExchange))
      continue;

    InstrumentInfo info;
    try {
      info.token = std::stoi(row[iExchToken]);
    } catch (...) {
      continue;
    }

    if (iTradSym >= 0 && (size_t)iTradSym < row.size())
      info.trading_symbol = row[iTradSym];
    if (iInstKey >= 0 && (size_t)iInstKey < row.size())
      info.instrument_key = row[iInstKey];
    if (iLotSize >= 0 && (size_t)iLotSize < row.size()) {
      try {
        info.lot_size = std::stoi(row[iLotSize]);
      } catch (...) {
      }
    }
    if (iTickSize >= 0 && (size_t)iTickSize < row.size()) {
      try {
        info.tick_size = std::stod(row[iTickSize]);
      } catch (...) {
      }
    }
    if (iName >= 0 && (size_t)iName < row.size())
      info.symbol = row[iName];

    // Upstox uses exchange names like NSE_FO, BSE_EQ, etc.
    std::string exch = row[iExchange];
    masters_["upstox"][exch][info.token] = std::move(info);
  }

  for (auto &[exch, m] : masters_["upstox"]) {
    std::cout << "[InstrumentNormalizer] Upstox " << exch << ": " << m.size()
              << " instruments loaded" << std::endl;
  }
}

void InstrumentNormalizer::loadFyersMaster(const std::string &dir, bool force) {
  // Fyers CSVs have no header row — column names from Python reference:
  // Fytoken,SymbolDetails,ExchangeInstrumentType,MinimumLotSize,TickSize,
  // ISIN,TradingSession,LastUpdateDate,Expirydate,SymbolTicker,Exchange,
  // Segment,ScripCode,UnderlyingSymbol,...
  const int COL_FYTOKEN = 0;
  const int COL_LOTSZ = 3;
  const int COL_TICKSZ = 4;
  const int COL_SYMTICKER = 9;
  const int COL_SCRIPCODE = 12;
  const int COL_UNDERLYING = 13;

  for (auto &[exchange, url] : FYERS_URLS) {
    std::string cachePath = dir + "/fyers_" + exchange + ".csv";
    std::string csvData;

    if (force || !fileExists(cachePath)) {
      std::cout << "[InstrumentNormalizer] Downloading Fyers " << exchange
                << " master..." << std::endl;
      csvData = downloadUrl(url);
      writeFile(cachePath, csvData);
    } else {
      std::cout << "[InstrumentNormalizer] Loading Fyers " << exchange
                << " from cache..." << std::endl;
      csvData = readFile(cachePath);
    }

    auto rows = parseCSV(csvData);
    auto &exchMap = masters_["fyers"][exchange];

    for (auto &row : rows) {
      if (row.size() <= (size_t)COL_UNDERLYING)
        continue;

      InstrumentInfo info;
      try {
        info.token = std::stoi(row[COL_SCRIPCODE]);
      } catch (...) {
        continue;
      }

      info.fytoken = row[COL_FYTOKEN];
      info.symbol_ticker = row[COL_SYMTICKER];
      info.symbol = row[COL_UNDERLYING];
      try {
        info.lot_size = std::stoi(row[COL_LOTSZ]);
      } catch (...) {
      }
      try {
        info.tick_size = std::stod(row[COL_TICKSZ]);
      } catch (...) {
      }

      exchMap[info.token] = std::move(info);
    }
    std::cout << "[InstrumentNormalizer] Fyers " << exchange << ": "
              << exchMap.size() << " instruments loaded" << std::endl;
  }
}

// ═════════════════════════════════════════════════════════════════════
//  InstrumentNormalizer — Public API
// ═════════════════════════════════════════════════════════════════════

void InstrumentNormalizer::loadMasters(const std::string &mastersDir,
                                       const std::vector<std::string> &brokers,
                                       bool forceDownload) {

  // Create directory if it doesn't exist (best-effort)
#ifdef _WIN32
  _mkdir(mastersDir.c_str());
#else
  mkdir(mastersDir.c_str(), 0755);
#endif

  for (auto &broker : brokers) {
    try {
      if (broker == "alice")
        loadAliceMaster(mastersDir, forceDownload);
      else if (broker == "angel")
        loadAngelMaster(mastersDir, forceDownload);
      else if (broker == "zerodha")
        loadZerodhaMaster(mastersDir, forceDownload);
      else if (broker == "upstox")
        loadUpstoxMaster(mastersDir, forceDownload);
      else if (broker == "fyers")
        loadFyersMaster(mastersDir, forceDownload);
      else
        std::cerr << "[InstrumentNormalizer] Unknown broker: " << broker
                  << std::endl;
    } catch (const std::exception &e) {
      std::cerr << "[InstrumentNormalizer] Failed to load " << broker << ": "
                << e.what() << std::endl;
    }
  }
}

const InstrumentInfo *InstrumentNormalizer::getInstrument(
    const std::string &broker, const std::string &exchange, int token) const {
  auto bIt = masters_.find(broker);
  if (bIt == masters_.end())
    return nullptr;

  // For upstox, try mapped exchange name (NFO→NSE_FO, etc.)
  std::string lookupExch = exchange;
  if (broker == "upstox") {
    auto mapIt = UPSTOX_EXCHANGE_MAP.find(exchange);
    if (mapIt != UPSTOX_EXCHANGE_MAP.end())
      lookupExch = mapIt->second;
  }

  auto eIt = bIt->second.find(lookupExch);
  if (eIt == bIt->second.end())
    return nullptr;

  auto tIt = eIt->second.find(token);
  if (tIt == eIt->second.end())
    return nullptr;

  return &tIt->second;
}

bool InstrumentNormalizer::isLoaded(const std::string &broker) const {
  return masters_.find(broker) != masters_.end() &&
         !masters_.at(broker).empty();
}

// ═════════════════════════════════════════════════════════════════════
//  OrderExecutor — loadMasters delegate
// ═════════════════════════════════════════════════════════════════════

void OrderExecutor::loadMasters(const std::string &mastersDir,
                                bool forceDownload) {
  normalizer_.loadMasters(mastersDir,
                          {"alice", "angel", "zerodha", "upstox", "fyers"},
                          forceDownload);
}

// ═════════════════════════════════════════════════════════════════════
//  OrderExecutor — Simplified placeOrder
//  Resolves instrument data from masters, multiplies qty by lot_size
//  for F&O, fills UniversalOrder, then dispatches to the full version.
// ═════════════════════════════════════════════════════════════════════

OrderResult OrderExecutor::placeOrder(
    const std::string &userId, const std::string &exchange, int exchange_token,
    const std::string &transaction_type, int quantity,
    const std::string &product, const std::string &order_type,
    const std::string &position_type, double price, const std::string &tag) {
  OrderResult result;
  result.user_id = userId;

  // 1. Look up the broker for this user
  auto it = registry_.find(userId);
  if (it == registry_.end()) {
    result.message = "User [" + userId + "] is not registered.";
    std::cerr << "[OrderExecutor] " << result.message << std::endl;
    return result;
  }

  const BrokerInfo &info = it->second;
  result.broker = info.broker_name;

  // 2. Resolve instrument data from Alice master (primary reference)
  const InstrumentInfo *aliceInst =
      normalizer_.getInstrument("alice", exchange, exchange_token);

  // Also try to get broker-specific instrument data
  std::string brokerLookup = info.broker_name;
  if (brokerLookup == "aliceblue")
    brokerLookup = "alice";
  else if (brokerLookup == "angelone")
    brokerLookup = "angel";
  else if (brokerLookup == "kiteconnect")
    brokerLookup = "zerodha";

  const InstrumentInfo *brokerInst =
      normalizer_.getInstrument(brokerLookup, exchange, exchange_token);

  // 3. Determine lot_size and trading_symbol
  int lotSize = 1;
  std::string tradingSymbol = "";
  std::string symbolToken = std::to_string(exchange_token);

  if (aliceInst) {
    lotSize = aliceInst->lot_size;
    tradingSymbol = aliceInst->trading_symbol;
  }

  // Override with broker-specific data if available
  if (brokerInst) {
    tradingSymbol = brokerInst->trading_symbol;
    if (brokerInst->lot_size > 0)
      lotSize = brokerInst->lot_size;
  }

  // 4. Calculate final quantity (multiply by lot_size for F&O)
  int finalQty = quantity;
  if (isDerivExchange(exchange) && lotSize > 1) {
    finalQty = quantity * lotSize;
  }

  // 5. For Upstox, use instrument_key instead of token
  if (info.broker_name == "upstox" && brokerInst &&
      !brokerInst->instrument_key.empty()) {
    symbolToken = brokerInst->instrument_key;
  }

  // 6. For Fyers, use SymbolTicker (e.g., "NSE:SBIN-EQ")
  if (info.broker_name == "fyers" && brokerInst &&
      !brokerInst->symbol_ticker.empty()) {
    tradingSymbol = brokerInst->symbol_ticker;
  }

  // 7. Build UniversalOrder and dispatch
  UniversalOrder order;
  order.exchange = toUpper(exchange);
  order.symbol_token = symbolToken;
  order.trading_symbol = tradingSymbol;
  order.transaction_type = toUpper(transaction_type);
  order.quantity = finalQty;
  order.product = toUpper(product);
  order.order_type = toUpper(order_type);
  order.position_type = toUpper(position_type);
  order.price = price;
  order.tag = tag;

  std::cout << "[OrderExecutor] Simplified placeOrder: "
            << "user=" << userId << " broker=" << info.broker_name
            << " exch=" << order.exchange << " token=" << exchange_token
            << " symbol=" << tradingSymbol << " side=" << order.transaction_type
            << " qty=" << order.quantity << " (lots=" << quantity
            << " x lotSize=" << lotSize << ")"
            << " product=" << order.product << " orderType=" << order.order_type
            << " posType=" << order.position_type << std::endl;

  return placeOrder(userId, order);
}

// ═════════════════════════════════════════════════════════════════════
//  OrderExecutor — Thread Pool Parallel Order Placement
//  Dispatches orders for multiple users concurrently using std::async.
//  Batches by max_workers_ to avoid spawning too many threads.
// ═════════════════════════════════════════════════════════════════════

std::vector<OrderResult> OrderExecutor::placeOrderParallel(
    const std::vector<UserOrder> &users, const std::string &exchange,
    int exchange_token, const std::string &transaction_type,
    int default_quantity, const std::string &product,
    const std::string &order_type, const std::string &position_type,
    double price, const std::string &tag) {

  std::vector<OrderResult> results(users.size());

  std::cout << "[OrderExecutor] placeOrderParallel: " << users.size()
            << " users, exchange=" << exchange << " token=" << exchange_token
            << " side=" << transaction_type << " product=" << product
            << " maxWorkers=" << max_workers_ << std::endl;

  auto startTime = std::chrono::steady_clock::now();

  // Process in batches of max_workers_
  for (size_t batchStart = 0; batchStart < users.size();
       batchStart += max_workers_) {

    size_t batchEnd = std::min(batchStart + (size_t)max_workers_, users.size());
    std::vector<std::future<OrderResult>> futures;

    // Launch async tasks for this batch
    for (size_t i = batchStart; i < batchEnd; ++i) {
      const auto &user = users[i];
      int qty = (user.quantity > 0) ? user.quantity : default_quantity;

      futures.push_back(std::async(
          std::launch::async,
          [this, &exchange, exchange_token, &transaction_type, qty, &product,
           &order_type, &position_type, price, &tag,
           userId = user.user_id]() -> OrderResult {
            try {
              return this->placeOrder(userId, exchange, exchange_token,
                                      transaction_type, qty, product,
                                      order_type, position_type, price, tag);
            } catch (const std::exception &ex) {
              OrderResult err;
              err.user_id = userId;
              err.success = false;
              err.message = std::string("Exception: ") + ex.what();
              return err;
            }
          }));
    }

    // Collect results from this batch
    for (size_t i = 0; i < futures.size(); ++i) {
      results[batchStart + i] = futures[i].get();
    }
  }

  auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                     std::chrono::steady_clock::now() - startTime)
                     .count();

  // Summary
  int successCount = 0, failCount = 0;
  for (auto &r : results) {
    if (r.success)
      ++successCount;
    else
      ++failCount;
  }

  std::cout << "[OrderExecutor] placeOrderParallel complete: " << successCount
            << " success, " << failCount << " failed, " << elapsed << "ms total"
            << std::endl;

  return results;
}

// ═════════════════════════════════════════════════════════════════════
//  Example main() — shows how to use OrderExecutor
// ═════════════════════════════════════════════════════════════════════

int main() {
  curl_global_init(CURL_GLOBAL_DEFAULT);

  // ... create other broker instances as needed

  AngelOneBroker angel("FZLDMjMp", "eyJhbGciOiJIUzUxMiJ9.eyJ1c2VybmFtZSI6IkoyNTI1MDUiLCJyb2xlcyI6MCwidXNlcnR5cGUiOiJVU0VSIiwidG9rZW4iOiJleUpoYkdjaU9pSlNVekkxTmlJc0luUjVjQ0k2SWtwWFZDSjkuZXlKMWMyVnlYM1I1Y0dVaU9pSmpiR2xsYm5RaUxDSjBiMnRsYmw5MGVYQmxJam9pZEhKaFpHVmZZV05qWlhOelgzUnZhMlZ1SWl3aVoyMWZhV1FpT2pFekxDSnpiM1Z5WTJVaU9pSXpJaXdpWkdWMmFXTmxYMmxrSWpvaVkyRTRaakJoTkdJdFl6QmtPQzB6T0RZNUxUZzRNVFl0T0RBNU1EQTJaRFZpTnpZMklpd2lhMmxrSWpvaWRISmhaR1ZmYTJWNVgzWXlJaXdpYjIxdVpXMWhibUZuWlhKcFpDSTZNVE1zSW5CeWIyUjFZM1J6SWpwN0ltUmxiV0YwSWpwN0luTjBZWFIxY3lJNkltRmpkR2wyWlNKOUxDSnRaaUk2ZXlKemRHRjBkWE1pT2lKaFkzUnBkbVVpZlgwc0ltbHpjeUk2SW5SeVlXUmxYMnh2WjJsdVgzTmxjblpwWTJVaUxDSnpkV0lpT2lKS01qVXlOVEExSWl3aVpYaHdJam94TnpjeU5ETTJNakUzTENKdVltWWlPakUzTnpJek5EazJNemNzSW1saGRDSTZNVGMzTWpNME9UWXpOeXdpYW5ScElqb2lNbUpsTVRsbFpqWXROVEkxT1MwME5UQXpMVGxoTVRFdFkyWmhPREEzTXpRelpEY3hJaXdpVkc5clpXNGlPaUlpZlEuZGtqQWZlOHVCRXhOeDRNN2FDNTg0X1FSV1h3SkhnQWwwNzhXNTlLa1FVTldDdm0zdy0xMHdOZW1IMEtpRjFHeklES3hPX0hTa1lCN3U5QkQ4UmZORFZleWZSWHRYQnBnNkpJRWx0NExFbHFXX3JFQ1ZjSVI0NnNJSGhqM3pPVUM0aGZWRVdjN3hkakVmVW53bVZiUEdOUnlsMkkzeDJ4VjRJVnlmaVVyVkRBIiwiQVBJLUtFWSI6IkZaTERNak1wIiwiWC1PTEQtQVBJLUtFWSI6dHJ1ZSwiaWF0IjoxNzcyMzQ5ODE3LCJleHAiOjE3NzIzODk4MDB9.8_NaN5qtNu5Iui24k2EqCeU9UjjpLmynPPrHe2YwAYkp9nAktIG-2F6ZipaNbiv62fdQhuBD9rqLyeQpWbXpsg", "J252505");
  AliceBlueBroker alice("eyJhbGciOiJSUzI1NiIsInR5cCIgOiAiSldUIiwia2lkIiA6ICIyam9lOFVScGxZU3FTcDB3RDNVemVBQkgxYkpmOE4wSDRDMGVVSWhXUVAwIn0.eyJleHAiOjE3Nzc1NDEzOTgsImlhdCI6MTc3MjM1NzQ4MywianRpIjoib25ydHJ0OjZlMjJmNDMzLWJkOWEtODgwMC0yYjgxLTc5MTJjMDE4MWJmNCIsImlzcyI6Imh0dHBzOi8vaWRhYXMuYWxpY2VibHVlb25saW5lLmNvbS9pZGFhcy9yZWFsbXMvQWxpY2VCbHVlIiwiYXVkIjoiYWNjb3VudCIsInN1YiI6IjVlZjY2YmZiLTRjNWQtNDM5OS1iNmM2LTdjODViNjE0NjU2ZiIsInR5cCI6IkJlYXJlciIsImF6cCI6ImFsaWNlLWtiIiwic2lkIjoiN2JjNjQ2MzQtMDRhNS1lYjFmLTc3MzItNjlhOWQ3NzM3M2MxIiwiYWxsb3dlZC1vcmlnaW5zIjpbImh0dHA6Ly9sb2NhbGhvc3Q6MzAwMiIsImh0dHA6Ly9sb2NhbGhvc3Q6NTA1MCIsImh0dHA6Ly9sb2NhbGhvc3Q6OTk0MyIsImh0dHA6Ly9sb2NhbGhvc3Q6OTAwMCJdLCJyZWFsbV9hY2Nlc3MiOnsicm9sZXMiOlsib2ZmbGluZV9hY2Nlc3MiLCJkZWZhdWx0LXJvbGVzLWFsaWNlYmx1ZWtiIiwidW1hX2F1dGhvcml6YXRpb24iXX0sInJlc291cmNlX2FjY2VzcyI6eyJhbGljZS1rYiI6eyJyb2xlcyI6WyJHVUVTVF9VU0VSIiwiQUNUSVZFX1VTRVIiXX0sImFjY291bnQiOnsicm9sZXMiOlsibWFuYWdlLWFjY291bnQiLCJtYW5hZ2UtYWNjb3VudC1saW5rcyIsInZpZXctcHJvZmlsZSJdfX0sInNjb3BlIjoiZW1haWwgcHJvZmlsZSBvcGVuaWQiLCJlbWFpbF92ZXJpZmllZCI6dHJ1ZSwidWNjIjoiMTgwNTY1NiIsImNsaWVudFJvbGUiOlsiR1VFU1RfVVNFUiIsIkFDVElWRV9VU0VSIl0sIm5hbWUiOiJKIFNhaSBLcmlzaG5hIiwicHJlZmVycmVkX3VzZXJuYW1lIjoiMTgwNTY1NiIsImdpdmVuX25hbWUiOiJKIFNhaSBLcmlzaG5hIn0.Mx1CGE_nnQualiAClgYKfq_4n4FTEdC6da5ojCzIA2Agua4QTX4iIqRimg04fhnnwyHWqFSFgJTfl4ZCkI7fj9Tc4zeDshPOPPkCezGoy0yMDjbwhL8GhWP6TUhrOCOfsrAVLb8IADffFdpMnAVzJjjsLkVBXmBhSriCcQuc7mS0MRLwZrAtbt2FbFBhnIq_nOYBK0ATPPoMutTynXK5l_M-q-0OHey4sNxf7K75jQKQmLCeF7n8X0nTucuqkjoqdsrwBWDkJtaYiawNkcq1pEjUwKkBBj_6kkS6mtYQ_6z8GoBdc02y1iggxUGOzjg3ahfqJabPYlX2UzZMWS8qnQ");
  KiteConnect     kite("h63xizkk69d4im7w", "6PlphqfUoQpvTgGd1h2DsGGKFG0O9x5m");

  // ... create other broker instances as needed

  // 2. Create the universal executor
  OrderExecutor executor;

  // 3. Register users → brokers
  executor.registerBroker("J252505", "angelone", &angel);
  executor.registerBroker("1805656", "aliceblue", &alice);
  executor.registerBroker("ILR269", "kiteconnect", &kite);

  // 4. Load instrument masters (downloads & caches CSV/JSON files)
  executor.loadMasters("./masters");

  // 5. Place order with simplified API — just 8 args!
  //    exchange, token, side, qty(lots), product, orderType, positionType
  // OrderResult r1 =
  //     executor.placeOrder("J252505", // userId
  //                         "NFO",     // exchange
  //                         35229,     // exchange_token
  //                         "BUY",     // transaction_type
  //                         1, // quantity (in lots, auto-multiplied by
  //                         lot_size) "MIS",    // product (CNC or MIS)
  //                         "MARKET", // order_type (MARKET or LIMIT)
  //                         "OPEN"    // position_type (OPEN or CLOSE)
  //     );
  // std::cout << "Angel => " << r1.message << std::endl;

  // Define users with per-user quantities
  std::vector<UserOrder> users = {
      {"J252505", 1}, // 1 lot for Angel user
      {"1805656", 2}, // 2 lots for Alice user
      {"ILR269", 1},  // 1 lot for Zerodha user
  };

  // Place for ALL users in parallel — one call!
  std::vector<OrderResult> results =
      executor.placeOrderParallel(users, // list of {userId, qty}
                                  "NFO", // exchange
                                  35229, // exchange_token
                                  "BUY", // transaction_type
                                  1, // default_quantity (for users with qty=0)
                                  "MIS",    // product
                                  "MARKET", // order_type
                                  "OPEN"    // position_type
      );

  // Check results
  for (auto &r : results) {
    std::cout << r.user_id << " => " << (r.success ? "OK" : "FAIL") << " "
              << r.message << std::endl;
  }

  curl_global_cleanup();
  return 0;
}
