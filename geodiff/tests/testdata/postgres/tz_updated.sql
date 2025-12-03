
DROP SCHEMA IF EXISTS gd_tz_updated CASCADE;

CREATE SCHEMA gd_tz_updated;

CREATE TABLE gd_tz_updated.simple ( "fid" SERIAL PRIMARY KEY, note TEXT, "created" TIMESTAMP WITHOUT TIME ZONE);

INSERT INTO gd_tz_updated.simple VALUES (1, 'row 1', '2021-10-28 18:34:19.472');
INSERT INTO gd_tz_updated.simple VALUES (2, 'row 2', '2021-10-28 18:34:19');
INSERT INTO gd_tz_updated.simple VALUES (3, 'row 3', '2021-10-28 18:34:19.53');
INSERT INTO gd_tz_updated.simple VALUES (4, 'row 4 updated', '2025-12-03 17:21:23.130895');
