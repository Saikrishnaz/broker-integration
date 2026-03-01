/**
 * OrderExecutor — Universal Order Execution Handler
 *
 * A single entry-point wrapper that accepts unified order arguments and
 * dispatches to the correct broker's placeOrder() based on the registered
 * user→broker mapping.  All broker-specific argument translation happens
 * inside this class; the individual broker .cpp files stay untouched.
 *
 * Supported brokers:
 *   AliceBlue, AngelOne, Dhan, Fyers, Findoc, Finvasia,
 *   5paisa, KiteConnect (Zerodha), Motilal Oswal, Upstox
 *
 * Dependencies  (same as the broker files):
 *   - libcurl, nlohmann/json, OpenSSL (for brokers that need it)
 *
 * Usage:
 *   OrderExecutor executor;
 *   executor.registerBroker("user123", "angelone", &angelBrokerInstance);
 *   UniversalOrder order;
 *   order.exchange         = "NSE";
 *   order.symbol_token     = "3045";
 *   order.trading_symbol   = "SBIN-EQ";
 *   order.transaction_type = "BUY";
 *   order.quantity          = 1;
 *   order.product           = "INTRADAY";
 *   order.order_type        = "MARKET";
 *   OrderResult res = executor.placeOrder("user123", order);
 */

#pragma once

#include <iostream>
#include <map>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

// ── Forward declarations for every broker class ─────────────────────
//    (so we don't need to #include every broker .cpp)
class AliceBlueBroker;
class AngelOneBroker;
class DhanBroker;
class FyersBroker;
class FindocBroker;
class FinvasiaBroker;
class FivepaisaBroker;
class KiteConnect;
class MotilalOswalBroker;
class UpstoxBroker;

using json = nlohmann::json;

// ─────────────────────────────────────────────────────────────────────
//  UniversalOrder — the single, unified order argument struct
// ─────────────────────────────────────────────────────────────────────

struct UniversalOrder {
  // ── Required fields ────────────────────────────
  std::string exchange;         // "NSE", "BSE", "NFO", "BFO", "MCX"
  std::string symbol_token;     // Exchange instrument token / scrip code
  std::string trading_symbol;   // Full trading symbol e.g. "SBIN-EQ"
  std::string transaction_type; // "BUY" or "SELL"
  int quantity = 0;             // Order quantity
  std::string product;          // "INTRADAY"/"CNC"/"NRML"/"MIS"/"MARGIN"
  std::string order_type;       // "MARKET"/"LIMIT"/"SL"/"SL-M"

  // ── Optional fields ────────────────────────────
  double price = 0.0;              // Limit price (0 for MARKET)
  double trigger_price = 0.0;      // Trigger price for SL orders
  std::string validity = "DAY";    // "DAY" / "IOC"
  int disclosed_qty = 0;           // Disclosed quantity
  std::string variety = "REGULAR"; // "REGULAR"/"AMO"/"BO"/"CO"
  std::string tag = "";            // Custom order tag
  bool is_amo = false;             // After-market order flag

  // ── 5paisa-specific (auto-derived if blank) ────
  std::string exchange_type = ""; // "C"(Cash) / "D"(Deriv) / "U"(Curr)
};

// ─────────────────────────────────────────────────────────────────────
//  OrderResult — standardised response from any broker
// ─────────────────────────────────────────────────────────────────────

struct OrderResult {
  bool success = false;
  std::string order_id = "";
  std::string broker = "";
  std::string user_id = "";
  std::string message = "";
  json raw_response; // full broker JSON for debugging
};

// ─────────────────────────────────────────────────────────────────────
//  BrokerInfo — internal record stored per registered user
// ─────────────────────────────────────────────────────────────────────

struct BrokerInfo {
  std::string broker_name; // lower-case canonical name
  void *broker_ptr;        // type-erased pointer to the broker instance
};

// ─────────────────────────────────────────────────────────────────────
//  OrderExecutor — the universal dispatcher
// ─────────────────────────────────────────────────────────────────────

class OrderExecutor {
public:
  OrderExecutor();
  ~OrderExecutor() = default;

  // ── Registration ─────────────────────────────────────────────────
  /**
   * Register a broker instance for a given user.
   *
   * @param userId      Unique identifier for the trading account
   * @param brokerName  One of: "aliceblue", "angelone", "dhan", "fyers",
   *                    "findoc", "finvasia", "5paisa", "kiteconnect",
   *                    "motilal", "upstox"
   * @param brokerPtr   Pointer to the constructed broker object
   *                    (must stay alive as long as OrderExecutor is used)
   */
  void registerBroker(const std::string &userId, const std::string &brokerName,
                      void *brokerPtr);

  // ── Order Placement ──────────────────────────────────────────────
  /**
   * Place an order for a registered user.
   * Translates UniversalOrder into the broker's native API format.
   *
   * @param userId  User whose broker should receive the order
   * @param order   Universal order parameters
   * @return        Standardised OrderResult
   */
  OrderResult placeOrder(const std::string &userId,
                         const UniversalOrder &order);

  // ── Utility ──────────────────────────────────────────────────────
  /** List all registered user IDs and their broker names. */
  std::vector<std::pair<std::string, std::string>> listRegistered() const;

  /** Check if a user is registered. */
  bool isRegistered(const std::string &userId) const;

private:
  std::map<std::string, BrokerInfo> registry_; // userId → BrokerInfo

  // ── Broker-specific dispatchers ──────────────────────────────────
  OrderResult placeOrderAliceBlue(AliceBlueBroker *b, const std::string &userId,
                                  const UniversalOrder &o);

  OrderResult placeOrderAngelOne(AngelOneBroker *b, const std::string &userId,
                                 const UniversalOrder &o);

  OrderResult placeOrderDhan(DhanBroker *b, const std::string &userId,
                             const UniversalOrder &o);

  OrderResult placeOrderFyers(FyersBroker *b, const std::string &userId,
                              const UniversalOrder &o);

  OrderResult placeOrderFindoc(FindocBroker *b, const std::string &userId,
                               const UniversalOrder &o);

  OrderResult placeOrderFinvasia(FinvasiaBroker *b, const std::string &userId,
                                 const UniversalOrder &o);

  OrderResult placeOrderFivepaisa(FivepaisaBroker *b, const std::string &userId,
                                  const UniversalOrder &o);

  OrderResult placeOrderKiteConnect(KiteConnect *b, const std::string &userId,
                                    const UniversalOrder &o);

  OrderResult placeOrderMotilal(MotilalOswalBroker *b,
                                const std::string &userId,
                                const UniversalOrder &o);

  OrderResult placeOrderUpstox(UpstoxBroker *b, const std::string &userId,
                               const UniversalOrder &o);

  // ── Mapping helpers ──────────────────────────────────────────────
  static std::string toLower(const std::string &s);
  static std::string toUpper(const std::string &s);

  // Product mapping: universal → broker-specific (exchange-dependent)
  // CNC maps to different values depending on whether it's equity or F&O
  static std::string mapProductAliceBlue(const std::string &product,
                                         const std::string &exchange);
  static std::string mapProductAngelOne(const std::string &product,
                                        const std::string &exchange);
  static std::string mapProductDhan(const std::string &product,
                                    const std::string &exchange);
  static std::string mapProductFyers(const std::string &product,
                                     const std::string &exchange);
  static std::string mapProductFindoc(const std::string &product,
                                      const std::string &exchange);
  static std::string mapProductFinvasia(const std::string &product,
                                        const std::string &exchange);
  static std::string mapProductFivepaisa(const std::string &product,
                                         const std::string &exchange);
  static std::string mapProductKite(const std::string &product,
                                    const std::string &exchange);
  static std::string mapProductMotilal(const std::string &product,
                                       const std::string &exchange);
  static std::string mapProductUpstox(const std::string &product,
                                      const std::string &exchange);

  // Helper: is exchange a derivatives segment?
  static bool isDerivExchange(const std::string &exchange);

  // OrderType mapping: universal → broker-specific
  static std::string mapOrderTypeAngelOne(const std::string &ot);
  static std::string mapOrderTypeDhan(const std::string &ot);
  static int mapOrderTypeFyers(const std::string &ot);
  static std::string mapOrderTypeFinvasia(const std::string &ot);
  static std::string mapOrderTypeMotilal(const std::string &ot);

  // Exchange mapping: universal → broker-specific
  static std::string mapExchangeDhan(const std::string &exch);
  static std::string mapExchangeFindoc(const std::string &exch);
  static std::string mapExchangeFivepaisa(const std::string &exch);
  static std::string mapExchangeTypeFivepaisa(const std::string &exch);

  // Transaction type mapping
  static int mapSideFyers(const std::string &side);
  static std::string mapSideFinvasia(const std::string &side);
  static std::string mapSideFivepaisa(const std::string &side);
  static std::string mapSideMotilal(const std::string &side);
};
