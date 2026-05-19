#include "../include/Persistence.h"
#include <sqlite3.h>
#include <iostream>
#include <chrono>

class PersistenceSqlite : public Persistence
{
public:
    PersistenceSqlite() : db_(nullptr) {}
    ~PersistenceSqlite() { if (db_) sqlite3_close(db_); }

    bool init(const std::string& dbPath) override
    {
        if (dbPath.empty()) return false;
        int rc = sqlite3_open(dbPath.c_str(), &db_);
        if (rc != SQLITE_OK) {
            std::cerr << "Failed to open sqlite db: " << sqlite3_errmsg(db_) << std::endl;
            sqlite3_close(db_);
            db_ = nullptr;
            return false;
        }

        const char* schema =
            "CREATE TABLE IF NOT EXISTS rooms(room_id INTEGER PRIMARY KEY, metadata TEXT);"
            "CREATE TABLE IF NOT EXISTS memberships(room_id INTEGER, username TEXT, PRIMARY KEY(room_id, username));"
            "CREATE TABLE IF NOT EXISTS messages_audit(id INTEGER PRIMARY KEY AUTOINCREMENT, room_id INTEGER, sender TEXT, message TEXT, ts INTEGER);"
            "CREATE TABLE IF NOT EXISTS offline_messages(id INTEGER PRIMARY KEY AUTOINCREMENT, room_id INTEGER, recipient TEXT, sender TEXT, message TEXT, ts INTEGER);";

        char* err = nullptr;
        rc = sqlite3_exec(db_, schema, nullptr, nullptr, &err);
        if (rc != SQLITE_OK) {
            std::cerr << "Failed to create schema: " << (err ? err : "") << std::endl;
            if (err) sqlite3_free(err);
            return false;
        }

        return true;
    }

    bool addMembership(int roomId, const std::string& username) override
    {
        if (!db_) return false;
        const char* sql = "INSERT OR IGNORE INTO memberships(room_id, username) VALUES(?,?)";
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;
        sqlite3_bind_int(stmt, 1, roomId);
        sqlite3_bind_text(stmt, 2, username.c_str(), -1, SQLITE_TRANSIENT);
        int rc = sqlite3_step(stmt);
        sqlite3_finalize(stmt);
        return rc == SQLITE_DONE;
    }

    std::vector<std::string> getRoomMembers(int roomId) override
    {
        std::vector<std::string> res;
        if (!db_) return res;
        const char* sql = "SELECT username FROM memberships WHERE room_id = ?";
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return res;
        sqlite3_bind_int(stmt, 1, roomId);
        while (sqlite3_step(stmt) == SQLITE_ROW)
        {
            const unsigned char* text = sqlite3_column_text(stmt, 0);
            if (text) res.emplace_back(reinterpret_cast<const char*>(text));
        }
        sqlite3_finalize(stmt);
        return res;
    }

    bool appendAudit(int roomId, const std::string& sender, const std::string& message, long long ts) override
    {
        if (!db_) return false;
        const char* sql = "INSERT INTO messages_audit(room_id, sender, message, ts) VALUES(?,?,?,?)";
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;
        sqlite3_bind_int(stmt, 1, roomId);
        sqlite3_bind_text(stmt, 2, sender.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 3, message.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int64(stmt, 4, ts);
        int rc = sqlite3_step(stmt);
        sqlite3_finalize(stmt);
        return rc == SQLITE_DONE;
    }

    bool storeOfflineMessage(int roomId, const std::string& recipient, const std::string& sender, const std::string& message, long long ts) override
    {
        if (!db_) return false;
        const char* sql = "INSERT INTO offline_messages(room_id, recipient, sender, message, ts) VALUES(?,?,?,?,?)";
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;
        sqlite3_bind_int(stmt, 1, roomId);
        sqlite3_bind_text(stmt, 2, recipient.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 3, sender.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 4, message.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int64(stmt, 5, ts);
        int rc = sqlite3_step(stmt);
        sqlite3_finalize(stmt);
        return rc == SQLITE_DONE;
    }

    std::vector<OfflineMessage> getOfflineMessages(const std::string& username) override
    {
        std::vector<OfflineMessage> res;
        if (!db_) return res;
        const char* sql = "SELECT id, room_id, recipient, sender, message, ts FROM offline_messages WHERE recipient = ? ORDER BY id";
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return res;
        sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_TRANSIENT);
        while (sqlite3_step(stmt) == SQLITE_ROW)
        {
            OfflineMessage m{};
            m.id = sqlite3_column_int(stmt, 0);
            m.roomId = sqlite3_column_int(stmt, 1);
            m.recipient = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
            m.sender = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
            m.message = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
            m.ts = sqlite3_column_int64(stmt, 5);
            res.push_back(std::move(m));
        }
        sqlite3_finalize(stmt);
        return res;
    }

    bool clearOfflineMessagesFor(const std::string& username) override
    {
        if (!db_) return false;
        const char* sql = "DELETE FROM offline_messages WHERE recipient = ?";
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;
        sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_TRANSIENT);
        int rc = sqlite3_step(stmt);
        sqlite3_finalize(stmt);
        return rc == SQLITE_DONE || rc == SQLITE_OK;
    }

private:
    sqlite3* db_;
};

// Factory function
Persistence* CreateSqlitePersistence(const std::string& dbPath)
{
    auto* p = new PersistenceSqlite();
    if (!p->init(dbPath))
    {
        delete p;
        return nullptr;
    }
    return p;
}
