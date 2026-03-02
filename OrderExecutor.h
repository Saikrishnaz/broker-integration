/**
 * OrderExecutor — Universal Order Execution Handler
 *
 * A single entry-point wrapper that accepts unified order arguments and
 * dispatches to the correct broker's placeOrder() based on the registered
 * user→broker mapping.  All broker-specific argument translation happens
 * inside this class; the individual broker .cpp files stay untouched.
 *
 * Includes InstrumentNormalizer for O(1) instrument data lookup from
 * broker master CSV/JSON files.
 *
 * Supported brokers:
 *   AliceBlue, AngelOne, Dhan, Fyers, Findoc, Finvasia,
 *   5paisa, KiteConnect (Zerodha), Motilal Oswal, Upstox
 *
 * Dependencies:
 *   - libcurl, nlohmann/json, OpenSSL (for brokers that need it)
 *
 * Simplified Usage:
 *   OrderExecutor executor;
 *   executor.registerBroker("J252505", "angelone", &angel);
 *   executor.loadMasters("./masters");  // download & cache master files
 *   OrderResult r = executor.placeOrder(
 *       "J252505", "NFO", 35229, "BUY", 1, "CNC", "MARKET", "OPEN");
 */

#pragma once

#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <fstream>
#include <future>
#include <iostream>
#include <map>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <unordered_map>
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
  int quantity = 0;             // Order quantity (in lots for F&O)
  std::string product;          // "CNC" or "MIS"
  std::string order_type;       // "MARKET" or "LIMIT"
  std::string position_type;    // "OPEN" or "CLOSE"

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
//  InstrumentInfo — data resolved from broker master files
// ─────────────────────────────────────────────────────────────────────

struct InstrumentInfo {
  int token = 0;                   // exchange token (the key)
  std::string trading_symbol = ""; // broker-specific symbol
  std::string symbol = "";         // underlying (NIFTY, SBIN, etc.)
  int lot_size = 1;                // lot size for qty multiplication
  double tick_size = 0.05;         // minimum price increment
  std::string instrument_key = ""; // upstox instrument_key
  std::string fytoken = "";        // fyers Fytoken
  std::string symbol_ticker = "";  // fyers SymbolTicker
  int instrument_token = 0;        // zerodha instrument_token
};

// ─────────────────────────────────────────────────────────────────────
//  InstrumentNormalizer — fast O(1) instrument lookup from master files
// ─────────────────────────────────────────────────────────────────────

class InstrumentNormalizer {
public:
  // Master file URLs (same as Python reference)
  static const std::map<std::string, std::string> ALICE_URLS;
  static const std::map<std::string, std::string> FYERS_URLS;
  static const std::string ANGEL_URL;
  static const std::string ZERODHA_URL;
  static const std::string UPSTOX_URL;
  static const std::map<std::string, std::string> UPSTOX_EXCHANGE_MAP;

  /**
   * Download and parse all broker master files.
   * @param mastersDir  Directory to cache CSV/JSON files
   * @param brokers     Which brokers to load (default: all 5)
   * @param forceDownload  Re-download even if cached
   */
  void loadMasters(const std::string &mastersDir,
                   const std::vector<std::string> &brokers = {"alice", "angel",
                                                              "zerodha",
                                                              "upstox",
                                                              "fyers"},
                   bool forceDownload = false);

  /**
   * O(1) lookup: get instrument info for a broker+exchange+token.
   * @return pointer to InstrumentInfo, or nullptr if not found
   */
  const InstrumentInfo *getInstrument(const std::string &broker,
                                      const std::string &exchange,
                                      int token) const;

  /** Check if masters are loaded for a broker. */
  bool isLoaded(const std::string &broker) const;

private:
  // Storage: masters_[broker][exchange][token] → InstrumentInfo
  std::unordered_map<
      std::string,
      std::unordered_map<std::string, std::unordered_map<int, InstrumentInfo>>>
      masters_;

  // Load individual broker masters (CSV/JSON → binary cache)
  void loadAliceMaster(const std::string &dir, bool force);
  void loadAngelMaster(const std::string &dir, bool force);
  void loadZerodhaMaster(const std::string &dir, bool force);
  void loadUpstoxMaster(const std::string &dir, bool force);
  void loadFyersMaster(const std::string &dir, bool force);

  // Binary serialization for instant loading
  // Format: [magic 4B][version 4B][count 4B] then per-record:
  //   token(4B) lot_size(4B) inst_token(4B) tick_size(8B)
  //   then 5 length-prefixed strings (trading_symbol, symbol,
  //   instrument_key, fytoken, symbol_ticker)
  void saveBinary(const std::string &path,
                  const std::unordered_map<int, InstrumentInfo> &data);
  bool loadBinary(const std::string &path,
                  std::unordered_map<int, InstrumentInfo> &data);

  // Helpers
  static std::string downloadUrl(const std::string &url);
  static std::vector<std::vector<std::string>>
  parseCSV(const std::string &data);
  static int findColumn(const std::vector<std::string> &header,
                        const std::string &name);
  static bool fileExists(const std::string &path);
  static void writeFile(const std::string &path, const std::string &data);
  static std::string readFile(const std::string &path);
};

// ─────────────────────────────────────────────────────────────────────
//  UserOrder — per-user entry for parallel order placement
// ─────────────────────────────────────────────────────────────────────

struct UserOrder {
  std::string user_id; // registered userId
  int quantity = 0;    // qty in lots for this user (0 = use default)
};

// ─────────────────────────────────────────────────────────────────────
//  OrderResult — normalised response from any broker
// ─────────────────────────────────────────────────────────────────────

struct OrderResult {
  // Core fields
  bool success = false;
  std::string status = "FAILED"; // SUCCESS | FAILED | REJECTED
  std::string order_id = "";
  std::string broker = "";
  std::string user_id = "";
  std::string message = "";
  std::string error = "";

  // Order context (filled by simplified placeOrder)
  std::string exchange = "";
  int exchange_token = 0;
  std::string trading_symbol = "";
  std::string transaction_type = "";
  std::string product = "";
  std::string order_type = "";
  std::string position_type = "";
  int placed_qty = 0;    // actual qty sent to broker
  int requested_qty = 0; // qty user asked for (lots)
  double price = 0.0;
  std::string tag = "";

  // Timing
  long long duration_ms = 0; // time taken to place order

  // Raw broker response
  json raw_response;
};

// ─────────────────────────────────────────────────────────────────────
//  TradeLogger — ASYNC CSV trade records + file logging
//  logTrade() and log() push to an internal queue (near-zero latency).
//  A background daemon thread drains the queue and writes to disk.
//  This ensures order execution is NEVER blocked by file I/O.
// ─────────────────────────────────────────────────────────────────────

class TradeLogger {
public:
  TradeLogger();
  ~TradeLogger();

  // Non-copyable, non-movable (owns a thread)
  TradeLogger(const TradeLogger &) = delete;
  TradeLogger &operator=(const TradeLogger &) = delete;

  /**
   * Initialise the logger.
   * @param logsDir       Directory for log files and CSV records
   * @param strategyName  Optional strategy name (used in file naming)
   */
  void init(const std::string &logsDir, const std::string &strategyName = "");

  /** Log a completed order result — NON-BLOCKING (pushes to queue). */
  void logTrade(const OrderResult &r);

  /** Write an info/warning/error log line — NON-BLOCKING. */
  void log(const std::string &level, const std::string &message);

  /** Flush all pending writes (blocks until queue is empty). */
  void flush();

  /** Get path to the CSV records file. */
  std::string getRecordsPath() const { return records_path_; }

  /** Get path to the log file. */
  std::string getLogPath() const { return log_path_; }

private:
  // Queue entry types
  struct LogEntry {
    bool is_trade = false; // true = CSV trade record
    std::string csv_line;  // pre-formatted CSV line
    std::string log_line;  // pre-formatted log line
  };

  std::string records_path_;
  std::string log_path_;
  std::string strategy_name_;
  bool initialised_ = false;

  // Async queue + background writer
  std::queue<LogEntry> queue_;
  std::mutex queue_mutex_;
  std::condition_variable queue_cv_;
  std::thread writer_thread_;
  bool stop_ = false;

  void writerLoop(); // background thread function
  static std::string timestamp();
  static std::string sanitize(const std::string &s);
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

  // ── Instrument Master Loading ────────────────────────────────────
  /**
   * Download and cache broker master files for instrument lookup.
   * Must be called once before using the simplified placeOrder.
   */
  void loadMasters(const std::string &mastersDir, bool forceDownload = false);

  // ── Trade Logger ──────────────────────────────────────────────────
  /**
   * Initialise the trade logger for CSV records and log files.
   * Call before placing orders to enable trade logging.
   * @param logsDir       Directory for log/CSV files (auto-created)
   * @param strategyName  Optional strategy name for file naming
   */
  void initLogger(const std::string &logsDir,
                  const std::string &strategyName = "");

  /** Get the TradeLogger (read-only). */
  const TradeLogger &getLogger() const { return logger_; }

  /** Get the TradeLogger (mutable, for flush()). */
  TradeLogger &getLogger() { return logger_; }

  // ── Order Placement (Full) ────────────────────────────────────────
  /**
   * Place an order for a registered user (full UniversalOrder version).
   */
  OrderResult placeOrder(const std::string &userId,
                         const UniversalOrder &order);

  // ── Order Placement (Simplified) ──────────────────────────────────
  /**
   * Place an order with minimal arguments. Instrument data is
   * auto-resolved from master files, quantity auto-multiplied
   * by lot_size for NFO/BFO.
   *
   * @param userId          Registered user ID
   * @param exchange        "NSE", "BSE", "NFO", "BFO", "MCX"
   * @param exchange_token  Exchange instrument token (int)
   * @param transaction_type "BUY" or "SELL"
   * @param quantity        Qty in lots (auto-multiplied for F&O)
   * @param product         "CNC" or "MIS"
   * @param order_type      "MARKET" or "LIMIT"
   * @param position_type   "OPEN" or "CLOSE"
   * @param price           Limit price (0 for MARKET)
   * @param tag             Optional order tag
   */
  OrderResult placeOrder(const std::string &userId, const std::string &exchange,
                         int exchange_token,
                         const std::string &transaction_type, int quantity,
                         const std::string &product,
                         const std::string &order_type,
                         const std::string &position_type, double price = 0.0,
                         const std::string &tag = "");

  // ── Thread Pool Parallel Order Placement ──────────────────────────
  /**
   * Place orders for MULTIPLE users in parallel using a thread pool.
   * All users get the same instrument/product/orderType/side — only
   * userId and quantity vary per user.
   *
   * @param users            List of {userId, quantity} pairs
   * @param exchange         "NSE", "BSE", "NFO", "BFO", "MCX"
   * @param exchange_token   Exchange instrument token
   * @param transaction_type "BUY" or "SELL"
   * @param default_quantity Default qty (used if user qty == 0)
   * @param product          "CNC" or "MIS"
   * @param order_type       "MARKET" or "LIMIT"
   * @param position_type    "OPEN" or "CLOSE"
   * @param price            Limit price (0 for MARKET)
   * @param tag              Optional order tag
   * @return                 Vector of OrderResult (one per user)
   */
  std::vector<OrderResult>
  placeOrderParallel(const std::vector<UserOrder> &users,
                     const std::string &exchange, int exchange_token,
                     const std::string &transaction_type, int default_quantity,
                     const std::string &product, const std::string &order_type,
                     const std::string &position_type, double price = 0.0,
                     const std::string &tag = "");

  /** Set max concurrent threads for parallel order placement. */
  void setMaxWorkers(int n) { max_workers_ = n; }

  // ── Utility ──────────────────────────────────────────────────────
  /** List all registered user IDs and their broker names. */
  std::vector<std::pair<std::string, std::string>> listRegistered() const;

  /** Check if a user is registered. */
  bool isRegistered(const std::string &userId) const;

private:
  std::map<std::string, BrokerInfo> registry_; // userId → BrokerInfo
  InstrumentNormalizer normalizer_;            // instrument master data
  TradeLogger logger_;                         // trade CSV + file logger
  int max_workers_ = 20;                       // max parallel threads
  std::mutex order_mutex_;                     // protects result collection

  // Response normalization helpers
  static std::string normalizeStatus(const std::string &rawStatus);
  void fillResultFromRaw(OrderResult &res);

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
