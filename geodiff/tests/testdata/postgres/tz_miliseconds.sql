
DROP SCHEMA IF EXISTS gd_tz_base CASCADE;

CREATE SCHEMA gd_tz_base;

CREATE TABLE gd_tz_base.simple ( "fid" SERIAL PRIMARY KEY, note TEXT, "created" TIMESTAMP WITHOUT TIME ZONE);

INSERT INTO gd_tz_base.simple VALUES (1, 'row 1', '2021-10-28 18:34:19.474');
INSERT INTO gd_tz_base.simple VALUES (2, 'row 2', '2021-10-28 18:34:19.476');
INSERT INTO gd_tz_base.simple VALUES (3, 'row 3');
INSERT INTO gd_tz_base.simple VALUES (4, 'row 4', '2025-12-03 17:21:23.130895');
