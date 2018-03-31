-- init for mysql
INSERT INTO Config(realm) VALUES ("testing");
INSERT INTO Authorize(authname, authtype, authaccess) VALUES ("system", "SYSTEM", "LOCAL");

INSERT INTO Authorize(authname, authtype, authaccess) VALUES("nobody", "SYSTEM", "LOCAL");
INSERT INTO Authorize(authname, authtype, authaccess) VALUES("anonymous", "SYSTEM", "DISABLED");
INSERT INTO Authorize(authname, authtype, authaccess) VALUES("operators", "SYSTEM", "PILOT");
INSERT INTO Extensions(extnbr, authname, display) VALUES (0, "operators", "Operators");

-- test1 and test2, for extensions 101 and 102, password is testing
INSERT INTO Authorize(authname, authdigest, realm, secret, fullname, authtype, authaccess)
    VALUES('test1','MD5','testing','74d0a5bd38ed78708aacb9f056e40120','Test User #1','USER','LOCAL');
INSERT INTO Authorize(authname, authdigest, realm, secret, fullname, authtype, authaccess, email)
    VALUES('test2','MD5','testing','6d292c665b1ed72b8bfdbb5d45173d98','Test User #2','USER','REMOTE','test2@localhost');
INSERT INTO Authorize(authname, fullname, authtype, authaccess)
    VALUES('test', 'Test Group', 'GROUP', 'LOCAL');
INSERT INTO Extensions(extnbr, authname) VALUES(100, 'test');
INSERT INTO Extensions(extnbr, authname) VALUES(101, 'test1');
INSERT INTO Extensions(extnbr, authname) VALUES(102, 'test2');
INSERT INTO Endpoints(extnbr) VALUES(101);
INSERT INTO Endpoints(extnbr) VALUES(102);
INSERT INTO Groups(grpnbr, extnbr) VALUES(100, 101);
INSERT INTO Groups(grpnbr, extnbr) VALUES(100, 102);

-- 101 sysop and group admin for testing
INSERT INTO Admin(authname, extnbr) VALUES('system', 101);
INSERT INTO Admin(authname, extnbr) VALUES('test', 101);

