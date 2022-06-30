
DROP SCHEMA IF EXISTS gd_datatypes CASCADE;
DROP SCHEMA IF EXISTS gd_datatypes_copy CASCADE;

CREATE SCHEMA gd_datatypes;

CREATE TABLE gd_datatypes.simple (
    "fid" SERIAL PRIMARY KEY,
    "geometry" GEOMETRY(POINT, 4326),
    "name_text" text,
    "name_varchar" character varying,
    "name_varchar_len" character varying(50),
    "name_char_len" character(100),
    "feature_id" uuid DEFAULT uuid_generate_v4(),
    "col_numeric" numeric(10,3)
);

INSERT INTO gd_datatypes.simple (
    "fid",
    "geometry",
    "name_text",
    "name_varchar",
    "name_varchar_len",
    "name_char_len",
    "col_numeric"
)
VALUES (
    1,
    ST_GeomFromText('Point (-1.08891928864569065 0.46101231190150482)', 4326),
    'feature1',
    'feature1 varchar',
    'feature1 varchar(50)',
    'feature1 char(100)',
    31.203
);
