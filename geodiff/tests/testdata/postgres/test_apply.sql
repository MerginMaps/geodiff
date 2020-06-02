
DROP SCHEMA IF EXISTS gd_test_apply CASCADE;

CREATE SCHEMA gd_test_apply;

ALTER TABLE gd_base.simple SET SCHEMA gd_test_apply;
