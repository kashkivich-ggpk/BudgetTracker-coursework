\set ON_ERROR_STOP on

SELECT format(
    'CREATE ROLE budget_tracker_app LOGIN PASSWORD %L',
    :'app_password'
)
WHERE NOT EXISTS (
    SELECT 1 FROM pg_roles WHERE rolname = 'budget_tracker_app'
)
\gexec

ALTER ROLE budget_tracker_app
    WITH LOGIN
    PASSWORD :'app_password'
    NOSUPERUSER
    NOCREATEDB
    NOCREATEROLE;

SELECT
    'CREATE DATABASE budget_tracker OWNER budget_tracker_app'
WHERE NOT EXISTS (
    SELECT 1 FROM pg_database WHERE datname = 'budget_tracker'
)
\gexec

\connect budget_tracker

ALTER DATABASE budget_tracker OWNER TO budget_tracker_app;
GRANT ALL ON SCHEMA public TO budget_tracker_app;

\echo PostgreSQL setup completed successfully.
