
DROP SCHEMA IF EXISTS gd_inserted_1_a CASCADE;

CREATE SCHEMA gd_inserted_1_a;

CREATE TABLE gd_inserted_1_a.simple ( "fid" SERIAL PRIMARY KEY, "geometry" GEOMETRY(POINT, 4326), "name" TEXT, "rating" INTEGER);

INSERT INTO gd_inserted_1_a.simple VALUES (1, ST_GeomFromText('Point (-1.08891928864569065 0.46101231190150482)', 4326), 'feature1', 1);
INSERT INTO gd_inserted_1_a.simple VALUES (2, ST_GeomFromText('Point (-0.36388508891928861 0.56224350205198359)', 4326), 'feature2', 2);
INSERT INTO gd_inserted_1_a.simple VALUES (3, ST_GeomFromText('Point (-0.73050615595075241 0.04240766073871405)', 4326), 'feature3', 3);
INSERT INTO gd_inserted_1_a.simple VALUES (4, ST_GeomFromText('Point (-0.80989507554245277 0.35087659877358479)', 4326), 'my new point A', 1);
