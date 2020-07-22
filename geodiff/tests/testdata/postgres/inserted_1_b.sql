
DROP SCHEMA IF EXISTS gd_inserted_1_b CASCADE;

CREATE SCHEMA gd_inserted_1_b;

CREATE TABLE gd_inserted_1_b.simple ( "fid" SERIAL PRIMARY KEY, "geometry" GEOMETRY(POINT, 4326), "name" TEXT, "rating" INTEGER);

INSERT INTO gd_inserted_1_b.simple VALUES (1, ST_GeomFromText('Point (-1.08891928864569065 0.46101231190150482)', 4326), 'feature1', 1);
INSERT INTO gd_inserted_1_b.simple VALUES (2, ST_GeomFromText('Point (-0.36388508891928861 0.56224350205198359)', 4326), 'feature2', 2);
INSERT INTO gd_inserted_1_b.simple VALUES (3, ST_GeomFromText('Point (-0.73050615595075241 0.04240766073871405)', 4326), 'feature3', 3);
INSERT INTO gd_inserted_1_b.simple VALUES (4, ST_GeomFromText('Point (-0.83257081666079680 0.16881887690991337)', 4326), 'my new point B', 2);
