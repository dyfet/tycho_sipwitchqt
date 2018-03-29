CREATE TABLE Config (
    id INTEGER PRIMARY KEY,               -- rowid in sqlite
    realm VARCHAR(128) NOT NULL,          -- site realm
    series INTEGER DEFAULT 9,             -- site db series
    dialing VARCHAR(8) DEFAULT 'STD3');  -- site dialing plan

CREATE TABLE Switches (
    version VARCHAR(8) NOT NULL,          -- db series supported
    uuid CHAR(36) NOT NULL,               -- switch uuid
    last DATETIME,
    PRIMARY KEY (uuid)
    );


CREATE TABLE Authorize (
    name VARCHAR(32),                     -- authorizing user or group id
    type VARCHAR(8) DEFAULT 'USER',       -- group type
    digest VARCHAR(8) DEFAULT 'NONE',     -- digest format of secret
    realm VARCHAR(128),                   -- realm used for secret
    secret VARCHAR(128),                  -- secret to use
    access VARCHAR(8) DEFAULT 'LOCAL',    -- type of access allowed (local, remote, all)
    email VARCHAR(128),                   -- email contact to use (avatar, etc)
    fullname VARCHAR(64),                 -- display name
    fwd_away INTEGER DEFAULT -1,          -- forward offline/away
    fwd_busy INTEGER DEFAULT -1,          -- forward busy
    fwd_answer INTEGER DEFAULT -1,        -- forward no answer
    PRIMARY KEY (name)
);

CREATE TABLE Extensions (
    num INTEGER NOT NULL,              -- ext number
    name VARCHAR(32) DEFAULT '@nobody',   -- group affinity
    priority INTEGER DEFAULT 0,           -- ring/dial priority
    restric INTEGER DEFAULT 0,           -- outgoing restrictions
    description VARCHAR(64),                 -- location info
    display VARCHAR(64),                  -- can override group display
    PRIMARY KEY (num),
    FOREIGN KEY (name) REFERENCES Authorize(name)
        ON DELETE CASCADE
);

CREATE TABLE Endpoints (
    endpoint INTEGER PRIMARY KEY AUTO_INCREMENT,
    num INTEGER NOT NULL,              -- extension of endpoint
    label VARCHAR(32) DEFAULT 'NONE',      -- label id
    agent VARCHAR(64),                    -- agent id
    created DATETIME DEFAULT CURRENT_TIMESTAMP,
    last DATETIME DEFAULT 0,
    FOREIGN KEY (num) REFERENCES Extensions(num)
        ON DELETE CASCADE
);

CREATE UNIQUE INDEX Registry ON Endpoints(num, label);

-- the system speed dialing is system group 10-99
-- per extension group personal speed dials 01-09 (--1---9)

CREATE TABLE Speeds (
    name VARCHAR(32),                     -- speed dial for...
    target VARCHAR(128),                  -- local or external uri
    num INTEGER,                       -- speed dial --
    CONSTRAINT dialing PRIMARY KEY (name, num),
    FOREIGN KEY (name) REFERENCES Authorize(name)
        ON DELETE CASCADE
);

CREATE TABLE Calling (
    name VARCHAR(32),                     -- group hunt is part of
    num INTEGER,                       -- extension -- to ring
    priority INTEGER DEFAULT 0,           -- hunt group priority order
    FOREIGN KEY (name) REFERENCES Authorize(name)
        ON DELETE CASCADE,
    FOREIGN KEY (num) REFERENCES Extensions(num)
        ON DELETE CASCADE
);

CREATE TABLE Admin (
    name VARCHAR(32),                     -- group permission is for
    num INTEGER,                       -- extension with permission
    FOREIGN KEY (name) REFERENCES Authorize(name)
        ON DELETE CASCADE,
    FOREIGN KEY (num) REFERENCES Extensions(num)
        ON DELETE CASCADE
);

CREATE TABLE Groups (
    pilot INTEGER,                        -- group tied to
    mem INTEGER,                       -- group member extension
    priority INTEGER DEFAULT -1,          -- coverage priority
    CONSTRAINT group PRIMARY KEY (pilot, mem),
    FOREIGN KEY (pilot) REFERENCES Extension(num)  ON DELETE CASCADE,
    FOREIGN KEY (mem) REFERENCES Extensions(num) ON DELETE CASCADE
);

CREATE TABLE Providers (
    contact VARCHAR(128) NOT NULL,        -- provider host uri
    protocol VARCHAR(3) DEFAULT 'UDP',    -- providers usually udp
    userid VARCHAR(32) NOT NULL,          -- auth code
    passwd VARCHAR(128) NOT NULL,         -- password to hash from
    display VARCHAR(64) NOT NULL,         -- provider short name
    PRIMARY KEY (contact)
);

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
    msgtext TEXT
);


CREATE TABLE Outboxes (
    mid BIGINT,
    endpoint INTEGER,
    msgstatus INTEGER DEFAULT 0,          -- status code from stack
    CONSTRAINT outbox PRIMARY KEY (endpoint, mid),
    FOREIGN KEY (mid) REFERENCES Messages(mid)
        ON DELETE CASCADE,
    FOREIGN KEY (endpoint) REFERENCES Endpoints(endpoint)
        ON DELETE CASCADE
);

