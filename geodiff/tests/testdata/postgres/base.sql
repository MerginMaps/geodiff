
DROP SCHEMA IF EXISTS gd_base CASCADE;

CREATE SCHEMA gd_base;

CREATE TABLE gd_base.simple ( "fid" SERIAL PRIMARY KEY, "geometry" GEOMETRY, "name" TEXT, "rating" INTEGER);

INSERT INTO gd_base.simple VALUES (1, ST_GeomFromText('Point (-1.08891928864569065 0.46101231190150482)'), 'feature1', 1);
INSERT INTO gd_base.simple VALUES (2, ST_GeomFromText('Point (-0.36388508891928861 0.56224350205198359)'), 'feature2', 2);
INSERT INTO gd_base.simple VALUES (3, ST_GeomFromText('Point (-0.73050615595075241 0.04240766073871405)'), 'feature3', 3);
