CREATE TABLE Config (
    id INTEGER PRIMARY KEY,               -- rowid in sqlite
    realm VARCHAR(128) NOT NULL,          -- site realm
    dbseries INTEGER DEFAULT 9,             -- site db series
    dialplan VARCHAR(8) DEFAULT 'STD3');  -- site dialing plan

CREATE TABLE Switches (
    version VARCHAR(8) NOT NULL,          -- db series # supported
    uuid CHAR(36) NOT NULL,               -- switch uuid
    lastaccess DATETIME,
    PRIMARY KEY (uuid));

CREATE TABLE Authorize (
    authname VARCHAR(32),                     -- authorizing user or group id
    authtype VARCHAR(8) DEFAULT 'USER',       -- group type
    authdigest VARCHAR(8) DEFAULT 'NONE',     -- digest format of secret
    created DATETIME DEFAULT CURRENT_TIMESTAMP,
    realm VARCHAR(128),                   -- realm used for secret
    secret VARCHAR(128),                  -- secret to use
    authaccess VARCHAR(8) DEFAULT 'LOCAL',    -- type of access allowed (local, remote, all)
    email VARCHAR(128),                   -- email contact to use (avatar, etc)
    fullname VARCHAR(64),                 -- display name
    fwdaway INTEGER DEFAULT -1,          -- forward offline/away
    fwdbusy INTEGER DEFAULT -1,          -- forward busy
    fwdnoanswer INTEGER DEFAULT -1,        -- forward no answer
    PRIMARY KEY (authname));

CREATE TABLE Extensions (
    extnbr INTEGER NOT NULL,              -- ext number
    authname VARCHAR(32) DEFAULT '@nobody',   -- group affinity
    extpriority INTEGER DEFAULT 0,           -- ring/dial priority
    callaccess INTEGER DEFAULT 0,         -- outgoing restrictions
    extlocation VARCHAR(64),              -- location info
    display VARCHAR(64),                  -- can override group display
    PRIMARY KEY (extnbr),
    FOREIGN KEY (authname) REFERENCES Authorize(authname)
        ON DELETE CASCADE);

CREATE TABLE Endpoints (
    endpoint INTEGER PRIMARY KEY AUTO_INCREMENT,
    extnbr INTEGER NOT NULL,               -- extension of endpoint
    label VARCHAR(32) DEFAULT 'NONE',      -- label id
    agent VARCHAR(64),                    -- agent id
    created DATETIME DEFAULT CURRENT_TIMESTAMP,
    lastaccess DATETIME DEFAULT 0,
    FOREIGN KEY (extnbr) REFERENCES Extensions(extnbr)
        ON DELETE CASCADE);

CREATE UNIQUE INDEX Registry ON Endpoints(extnbr, label);

-- the system speed dialing is system group 10-99
-- per extension group personal speed dials 01-09 (#1-#9)

CREATE TABLE Speeds (
    authname VARCHAR(32),                     -- speed dial for...
    target VARCHAR(128),                  -- local or external uri
    extnbr INTEGER,                       -- speed dial #
    CONSTRAINT dialing PRIMARY KEY (authname, extnbr),
    FOREIGN KEY (authname) REFERENCES Authorize(authname)
        ON DELETE CASCADE);

CREATE TABLE Calling (
    authname VARCHAR(32),                     -- group hunt is part of
    extnbr INTEGER,                       -- extension # to ring
    extpriority INTEGER DEFAULT 0,           -- hunt group priority order
    FOREIGN KEY (authname) REFERENCES Authorize(authname)
        ON DELETE CASCADE,
    FOREIGN KEY (extnbr) REFERENCES Extensions(extnbr)
        ON DELETE CASCADE);

CREATE TABLE Admin (
    authname VARCHAR(32),                     -- group permission is for
    extnbr INTEGER,                       -- extension with permission
    FOREIGN KEY (authname) REFERENCES Authorize(authname)
        ON DELETE CASCADE,
    FOREIGN KEY (extnbr) REFERENCES Extensions(extnbr)
        ON DELETE CASCADE);

CREATE TABLE Groups (
    grpnbr INTEGER,                       -- group tied to
    extnbr INTEGER,                       -- group member extension
    extpriority INTEGER DEFAULT -1,          -- coverage priority
    CONSTRAINT Grouping PRIMARY KEY (grpnbr, extnbr),
    FOREIGN KEY (grpnbr) REFERENCES Extensions(extnbr)
        ON DELETE CASCADE,
    FOREIGN KEY (extnbr) REFERENCES Extensions(extnbr)
        ON DELETE CASCADE);

CREATE TABLE Providers (
    contact VARCHAR(128) NOT NULL,        -- provider host uri
    protocol VARCHAR(3) DEFAULT 'UDP',    -- providers usually udp
    userid VARCHAR(32) NOT NULL,          -- auth code
    passwd VARCHAR(128) NOT NULL,         -- password to hash from
    display VARCHAR(64) NOT NULL,         -- provider short name
    PRIMARY KEY (contact));

CREATE TABLE Messages (
    mid BIGINT PRIMARY KEY AUTO_INCREMENT,
    msgseq INTEGER,                       -- helps exclude dups on devices
    msgfrom VARCHAR(64),                  -- origin uri direct reply
    msgto VARCHAR(64),                    -- to for creating uri
    subject VARCHAR(80),                  -- subject header used
    display VARCHAR(64),                  -- origin display name
    posted TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    expires TIMESTAMP,                    -- estimated expiration
    msgtype VARCHAR(8),
    msgtext TEXT);

CREATE TABLE Deletes (
    authname VARCHAR(32),
    delstatus INTEGER DEFAULT 0,
    endpoint INTEGER,
    created DATETIME,
    FOREIGN KEY (endpoint) REFERENCES Endpoints(endpoint)
        ON DELETE CASCADE);

CREATE TABLE Outboxes (
    mid BIGINT,
    endpoint INTEGER,
    msgstatus INTEGER DEFAULT 0,          -- status code from stack
    CONSTRAINT outbox PRIMARY KEY (endpoint, mid),
    FOREIGN KEY (mid) REFERENCES Messages(mid)
        ON DELETE CASCADE,
    FOREIGN KEY (endpoint) REFERENCES Endpoints(endpoint)
        ON DELETE CASCADE);
