use rusqlite::{
    named_params,
    types::{FromSql, FromSqlResult, ToSql, ToSqlOutput, ValueRef},
    Connection,
};
use std::sync::Mutex;
use uuid::Uuid;

/// Wrapper so Uuid implements ToSql/FromSql without orphan rules.
/// Stored as TEXT in SQLite (the canonical hyphenated form).
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct SqlUuid(pub Uuid);

impl From<Uuid> for SqlUuid {
    fn from(u: Uuid) -> Self {
        Self(u)
    }
}

impl ToSql for SqlUuid {
    fn to_sql(&self) -> rusqlite::Result<ToSqlOutput<'_>> {
        Ok(ToSqlOutput::from(self.0.to_string()))
    }
}

impl FromSql for SqlUuid {
    fn column_result(value: ValueRef<'_>) -> FromSqlResult<Self> {
        let s = value.as_str()?;
        Uuid::parse_str(s)
            .map(SqlUuid)
            .map_err(|e| rusqlite::types::FromSqlError::Other(Box::new(e)))
    }
}

/// Row returned by `get_friends`.
pub struct FriendRow {
    pub friend_id: Uuid,
    pub callsign: String,
    pub status: String,
}

/// Data for inserting a match result.
pub struct MatchResultData {
    pub match_id: Uuid,
    pub lobby_id: Uuid,
    pub duration_secs: u32,
    pub game_mode: String,
    pub mission: String,
    pub result: String,
    pub player_count: u32,
}

/// Data for inserting a match player result.
pub struct MatchPlayerData {
    pub match_id: Uuid,
    pub player_id: Uuid,
    pub callsign: String,
    pub score: i32,
    pub kills: u32,
    pub deaths: u32,
    pub result: String,
}

/// Thin wrapper around SQLite with schema initialization.
pub struct Database {
    conn: Mutex<Connection>,
}

impl Database {
    pub fn open(path: &str) -> Result<Self, rusqlite::Error> {
        let conn = Connection::open(path)?;
        conn.execute_batch("PRAGMA journal_mode=WAL; PRAGMA foreign_keys=ON;")?;
        let db = Self {
            conn: Mutex::new(conn),
        };
        db.init_schema()?;
        Ok(db)
    }

    fn init_schema(&self) -> Result<(), rusqlite::Error> {
        let conn = self.conn.lock().unwrap();
        conn.execute_batch(
            "
            CREATE TABLE IF NOT EXISTS players (
                player_id      TEXT PRIMARY KEY,
                gpgs_player_id TEXT UNIQUE,
                callsign       TEXT NOT NULL,
                created_at     TEXT NOT NULL DEFAULT (datetime('now')),
                last_seen      TEXT NOT NULL DEFAULT (datetime('now')),
                total_playtime INTEGER NOT NULL DEFAULT 0,
                games_played   INTEGER NOT NULL DEFAULT 0
            );

            CREATE TABLE IF NOT EXISTS friends (
                player_id  TEXT NOT NULL REFERENCES players(player_id),
                friend_id  TEXT NOT NULL REFERENCES players(player_id),
                status     TEXT NOT NULL DEFAULT 'pending',
                created_at TEXT NOT NULL DEFAULT (datetime('now')),
                PRIMARY KEY (player_id, friend_id)
            );

            CREATE TABLE IF NOT EXISTS match_results (
                match_id      TEXT PRIMARY KEY,
                lobby_id      TEXT NOT NULL,
                started_at    TEXT NOT NULL DEFAULT (datetime('now')),
                ended_at      TEXT,
                duration_secs INTEGER,
                game_mode     TEXT,
                mission       TEXT,
                level         INTEGER,
                difficulty    INTEGER,
                result        TEXT,
                player_count  INTEGER
            );

            CREATE TABLE IF NOT EXISTS match_players (
                match_id       TEXT NOT NULL REFERENCES match_results(match_id),
                player_id      TEXT NOT NULL REFERENCES players(player_id),
                callsign       TEXT,
                score          INTEGER,
                kills          INTEGER,
                deaths         INTEGER,
                team           INTEGER,
                result         TEXT,
                connected_secs INTEGER,
                PRIMARY KEY (match_id, player_id)
            );

            CREATE TABLE IF NOT EXISTS player_count_history (
                timestamp TEXT PRIMARY KEY,
                online    INTEGER NOT NULL,
                in_game   INTEGER NOT NULL,
                lobbies   INTEGER NOT NULL
            );

            CREATE TABLE IF NOT EXISTS connection_events (
                id         INTEGER PRIMARY KEY AUTOINCREMENT,
                timestamp  TEXT NOT NULL DEFAULT (datetime('now')),
                player_id  TEXT,
                event_type TEXT NOT NULL,
                details    TEXT,
                nat_type   TEXT,
                latency_ms INTEGER
            );

            CREATE TABLE IF NOT EXISTS admin_log (
                id        INTEGER PRIMARY KEY AUTOINCREMENT,
                timestamp TEXT NOT NULL DEFAULT (datetime('now')),
                action    TEXT NOT NULL,
                details   TEXT
            );

            CREATE TABLE IF NOT EXISTS bans (
                player_id  TEXT PRIMARY KEY REFERENCES players(player_id),
                reason     TEXT,
                banned_at  TEXT NOT NULL DEFAULT (datetime('now')),
                expires_at TEXT
            );

            CREATE TABLE IF NOT EXISTS keypair_identities (
                pubkey_hash TEXT PRIMARY KEY,
                player_id   TEXT NOT NULL REFERENCES players(player_id),
                created_at  TEXT NOT NULL DEFAULT (datetime('now'))
            );
            ",
        )?;
        Ok(())
    }

    // -- Player operations --

    pub fn upsert_player(
        &self,
        player_id: &Uuid,
        gpgs_player_id: &str,
        callsign: &str,
    ) -> Result<(), rusqlite::Error> {
        let conn = self.conn.lock().unwrap();
        conn.prepare_cached(
            "INSERT INTO players (player_id, gpgs_player_id, callsign)
             VALUES (:id, :gpgs, :name)
             ON CONFLICT(player_id) DO UPDATE SET
                callsign = excluded.callsign,
                last_seen = datetime('now')",
        )?
        .execute(named_params! {
            ":id":   SqlUuid::from(*player_id),
            ":gpgs": gpgs_player_id,
            ":name": callsign,
        })?;
        Ok(())
    }

    pub fn find_player_by_callsign(&self, callsign: &str) -> Result<Option<Uuid>, rusqlite::Error> {
        let conn = self.conn.lock().unwrap();
        let mut stmt =
            conn.prepare_cached("SELECT player_id FROM players WHERE callsign = :name LIMIT 1")?;
        stmt.query_row(named_params! { ":name": callsign }, |row| {
            row.get::<_, SqlUuid>(0).map(|u| u.0)
        })
        .optional()
    }

    /// Look up a player by their GPGS player ID. If not found, create a new
    /// row with a stable UUID and return it. This ensures the same GPGS
    /// identity always maps to the same server-side player_id.
    pub fn find_or_create_player_by_gpgs(
        &self,
        gpgs_player_id: &str,
        callsign: &str,
    ) -> Result<Uuid, rusqlite::Error> {
        let conn = self.conn.lock().unwrap();

        // Try to find existing player by gpgs_player_id
        let existing: Option<Uuid> = conn
            .prepare_cached("SELECT player_id FROM players WHERE gpgs_player_id = :gpgs LIMIT 1")?
            .query_row(named_params! { ":gpgs": gpgs_player_id }, |row| {
                row.get::<_, SqlUuid>(0).map(|u| u.0)
            })
            .optional()?;

        if let Some(pid) = existing {
            // Update callsign and last_seen
            conn.prepare_cached(
                "UPDATE players SET callsign = :name, last_seen = datetime('now')
                 WHERE player_id = :id",
            )?
            .execute(named_params! {
                ":name": callsign,
                ":id":   SqlUuid::from(pid),
            })?;
            return Ok(pid);
        }

        // Create new player with stable UUID
        let pid = Uuid::new_v4();
        conn.prepare_cached(
            "INSERT INTO players (player_id, gpgs_player_id, callsign)
             VALUES (:id, :gpgs, :name)",
        )?
        .execute(named_params! {
            ":id":   SqlUuid::from(pid),
            ":gpgs": gpgs_player_id,
            ":name": callsign,
        })?;
        Ok(pid)
    }

    /// Look up a player by their keypair hash. Returns the player_id if found.
    pub fn find_player_by_keypair(
        &self,
        pubkey_hash: &str,
    ) -> Result<Option<Uuid>, rusqlite::Error> {
        let conn = self.conn.lock().unwrap();
        let mut stmt = conn.prepare_cached(
            "SELECT player_id FROM keypair_identities WHERE pubkey_hash = :hash LIMIT 1",
        )?;
        stmt.query_row(named_params! { ":hash": pubkey_hash }, |row| {
            row.get::<_, SqlUuid>(0).map(|u| u.0)
        })
        .optional()
    }

    /// Register a new keypair-authenticated player. Creates a player row and
    /// a keypair_identities row. Returns the new player_id.
    pub fn register_keypair_player(
        &self,
        pubkey_hash: &str,
        callsign: &str,
    ) -> Result<Uuid, rusqlite::Error> {
        let conn = self.conn.lock().unwrap();
        let pid = Uuid::new_v4();
        // Create player with a synthetic gpgs_player_id to satisfy the UNIQUE constraint.
        // Keypair players get a "keypair:" prefix to distinguish from real GPGS IDs.
        let synthetic_gpgs = format!("keypair:{pubkey_hash}");
        conn.prepare_cached(
            "INSERT INTO players (player_id, gpgs_player_id, callsign)
             VALUES (:id, :gpgs, :name)",
        )?
        .execute(named_params! {
            ":id":   SqlUuid::from(pid),
            ":gpgs": synthetic_gpgs,
            ":name": callsign,
        })?;
        conn.prepare_cached(
            "INSERT INTO keypair_identities (pubkey_hash, player_id) VALUES (:hash, :id)",
        )?
        .execute(named_params! {
            ":hash": pubkey_hash,
            ":id":   SqlUuid::from(pid),
        })?;
        Ok(pid)
    }

    pub fn is_banned(&self, player_id: &Uuid) -> Result<bool, rusqlite::Error> {
        let conn = self.conn.lock().unwrap();
        let mut stmt = conn.prepare_cached(
            "SELECT 1 FROM bans WHERE player_id = :id
             AND (expires_at IS NULL OR expires_at > datetime('now'))
             LIMIT 1",
        )?;
        stmt.exists(named_params! { ":id": SqlUuid::from(*player_id) })
    }

    /// Check if `blocker_id` has blocked `player_id`.
    pub fn is_blocked(&self, blocker_id: &Uuid, player_id: &Uuid) -> Result<bool, rusqlite::Error> {
        let conn = self.conn.lock().unwrap();
        let mut stmt = conn.prepare_cached(
            "SELECT 1 FROM friends
             WHERE player_id = :blocker AND friend_id = :target AND status = 'blocked'
             LIMIT 1",
        )?;
        stmt.exists(named_params! {
            ":blocker": SqlUuid::from(*blocker_id),
            ":target":  SqlUuid::from(*player_id),
        })
    }

    // -- Friends operations --

    pub fn add_friend_request(&self, from: &Uuid, to: &Uuid) -> Result<(), rusqlite::Error> {
        let conn = self.conn.lock().unwrap();
        conn.prepare_cached(
            "INSERT OR IGNORE INTO friends (player_id, friend_id, status)
             VALUES (:from, :to, 'pending')",
        )?
        .execute(named_params! {
            ":from": SqlUuid::from(*from),
            ":to":   SqlUuid::from(*to),
        })?;
        Ok(())
    }

    pub fn accept_friend(&self, player_id: &Uuid, friend_id: &Uuid) -> Result<(), rusqlite::Error> {
        let conn = self.conn.lock().unwrap();
        let tx = conn.unchecked_transaction()?;

        // Flip the existing pending row (friend_id -> player_id) to accepted
        tx.prepare_cached(
            "UPDATE friends SET status = 'accepted'
             WHERE player_id = :from AND friend_id = :to",
        )?
        .execute(named_params! {
            ":from": SqlUuid::from(*friend_id),
            ":to":   SqlUuid::from(*player_id),
        })?;

        // Ensure the reverse row exists as accepted
        tx.prepare_cached(
            "INSERT OR REPLACE INTO friends (player_id, friend_id, status)
             VALUES (:from, :to, 'accepted')",
        )?
        .execute(named_params! {
            ":from": SqlUuid::from(*player_id),
            ":to":   SqlUuid::from(*friend_id),
        })?;

        tx.commit()
    }

    pub fn remove_friend(&self, player_id: &Uuid, friend_id: &Uuid) -> Result<(), rusqlite::Error> {
        let conn = self.conn.lock().unwrap();
        conn.prepare_cached(
            "DELETE FROM friends
             WHERE (player_id = :a AND friend_id = :b)
                OR (player_id = :b AND friend_id = :a)",
        )?
        .execute(named_params! {
            ":a": SqlUuid::from(*player_id),
            ":b": SqlUuid::from(*friend_id),
        })?;
        Ok(())
    }

    pub fn block_player(&self, player_id: &Uuid, blocked_id: &Uuid) -> Result<(), rusqlite::Error> {
        let conn = self.conn.lock().unwrap();
        conn.prepare_cached(
            "INSERT OR REPLACE INTO friends (player_id, friend_id, status)
             VALUES (:id, :blocked, 'blocked')",
        )?
        .execute(named_params! {
            ":id":      SqlUuid::from(*player_id),
            ":blocked": SqlUuid::from(*blocked_id),
        })?;
        Ok(())
    }

    pub fn get_friends(&self, player_id: &Uuid) -> Result<Vec<FriendRow>, rusqlite::Error> {
        let conn = self.conn.lock().unwrap();
        let mut stmt = conn.prepare_cached(
            "SELECT f.friend_id, p.callsign, f.status
             FROM friends f
             JOIN players p ON p.player_id = f.friend_id
             WHERE f.player_id = :id",
        )?;
        let rows = stmt.query_map(named_params! { ":id": SqlUuid::from(*player_id) }, |row| {
            Ok(FriendRow {
                friend_id: row.get::<_, SqlUuid>(0)?.0,
                callsign: row.get(1)?,
                status: row.get(2)?,
            })
        })?;
        rows.collect()
    }

    // -- Match result operations --

    pub fn insert_match_result(&self, data: &MatchResultData) -> Result<(), rusqlite::Error> {
        let conn = self.conn.lock().unwrap();
        conn.prepare_cached(
            "INSERT INTO match_results
                (match_id, lobby_id, ended_at, duration_secs,
                 game_mode, mission, result, player_count)
             VALUES (:mid, :lid, datetime('now'), :dur, :mode, :mission, :result, :pc)",
        )?
        .execute(named_params! {
            ":mid":     SqlUuid::from(data.match_id),
            ":lid":     SqlUuid::from(data.lobby_id),
            ":dur":     data.duration_secs,
            ":mode":    data.game_mode.as_str(),
            ":mission": data.mission.as_str(),
            ":result":  data.result.as_str(),
            ":pc":      data.player_count,
        })?;
        Ok(())
    }

    pub fn insert_match_player(&self, data: &MatchPlayerData) -> Result<(), rusqlite::Error> {
        let conn = self.conn.lock().unwrap();
        conn.prepare_cached(
            "INSERT INTO match_players
                (match_id, player_id, callsign, score, kills, deaths, result)
             VALUES (:mid, :pid, :name, :score, :kills, :deaths, :result)",
        )?
        .execute(named_params! {
            ":mid":    SqlUuid::from(data.match_id),
            ":pid":    SqlUuid::from(data.player_id),
            ":name":   data.callsign.as_str(),
            ":score":  data.score,
            ":kills":  data.kills,
            ":deaths": data.deaths,
            ":result": data.result.as_str(),
        })?;
        Ok(())
    }

    // -- Stats snapshot --

    pub fn insert_player_count_snapshot(
        &self,
        online: u32,
        in_game: u32,
        lobbies: u32,
    ) -> Result<(), rusqlite::Error> {
        let conn = self.conn.lock().unwrap();
        conn.prepare_cached(
            "INSERT INTO player_count_history (timestamp, online, in_game, lobbies)
             VALUES (datetime('now'), :online, :in_game, :lobbies)",
        )?
        .execute(named_params! {
            ":online":  online,
            ":in_game": in_game,
            ":lobbies": lobbies,
        })?;
        Ok(())
    }

    pub fn total_unique_players(&self) -> Result<u64, rusqlite::Error> {
        let conn = self.conn.lock().unwrap();
        conn.query_row("SELECT COUNT(*) FROM players", [], |row| {
            row.get::<_, i64>(0).map(|n| n as u64)
        })
    }

    pub fn total_games_played(&self) -> Result<u64, rusqlite::Error> {
        let conn = self.conn.lock().unwrap();
        conn.query_row("SELECT COUNT(*) FROM match_results", [], |row| {
            row.get::<_, i64>(0).map(|n| n as u64)
        })
    }

    // -- Connection event logging --

    pub fn log_connection_event(
        &self,
        player_id: Option<&Uuid>,
        event_type: &str,
        details: Option<&str>,
    ) -> Result<(), rusqlite::Error> {
        let conn = self.conn.lock().unwrap();
        conn.prepare_cached(
            "INSERT INTO connection_events (player_id, event_type, details)
             VALUES (:id, :evt, :det)",
        )?
        .execute(named_params! {
            ":id":  player_id.map(|id| SqlUuid::from(*id)),
            ":evt": event_type,
            ":det": details,
        })?;
        Ok(())
    }

    // -- Admin --

    pub fn ban_player(
        &self,
        player_id: &Uuid,
        reason: &str,
        expires_at: Option<&str>,
    ) -> Result<(), rusqlite::Error> {
        let conn = self.conn.lock().unwrap();
        conn.prepare_cached(
            "INSERT OR REPLACE INTO bans (player_id, reason, expires_at)
             VALUES (:id, :reason, :exp)",
        )?
        .execute(named_params! {
            ":id":     SqlUuid::from(*player_id),
            ":reason": reason,
            ":exp":    expires_at,
        })?;
        Ok(())
    }

    pub fn unban_player(&self, player_id: &Uuid) -> Result<(), rusqlite::Error> {
        let conn = self.conn.lock().unwrap();
        conn.prepare_cached("DELETE FROM bans WHERE player_id = :id")?
            .execute(named_params! { ":id": SqlUuid::from(*player_id) })?;
        Ok(())
    }
}

use rusqlite::OptionalExtension;
