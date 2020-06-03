
DROP SCHEMA IF EXISTS gd_updated_a CASCADE;

CREATE SCHEMA gd_updated_a;

CREATE TABLE gd_updated_a.simple ( "fid" SERIAL PRIMARY KEY, "geometry" GEOMETRY(POINT, 4326), "name" TEXT, "rating" INTEGER);

INSERT INTO gd_updated_a.simple VALUES (1, ST_GeomFromText('Point (-1.08891928864569065 0.46101231190150482)', 4326), 'feature1', 1);
INSERT INTO gd_updated_a.simple VALUES (2, ST_GeomFromText('Point (-0.92836978235933398 0.23787002896191123)', 4326), 'feature2', 9999);
INSERT INTO gd_updated_a.simple VALUES (3, ST_GeomFromText('Point (-0.73050615595075241 0.04240766073871405)', 4326), 'feature3', 3);
