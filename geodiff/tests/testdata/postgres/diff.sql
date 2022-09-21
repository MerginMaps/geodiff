DROP SCHEMA IF EXISTS gd_pg_diff CASCADE;
CREATE SCHEMA gd_pg_diff;

CREATE TABLE gd_pg_diff.simple (
    "fid" SERIAL PRIMARY KEY,
    "geometry" GEOMETRY(POINT, 4326),
    "name" text
);

INSERT INTO gd_pg_diff.simple (
    "fid",
    "geometry",
    "name"
)
VALUES (
    1,
    ST_GeomFromText('Point (0 1)', 4326),
    'feature 1'
);
