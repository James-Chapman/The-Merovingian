#!/bin/bash
# Stand up a throwaway PostgreSQL 16 cluster mirroring .github/workflows/postgres-integration.yml
# so the postgres-integration failure can be reproduced locally.
set -eu
PGBIN=/usr/lib/postgresql/16/bin
PGDATA=/tmp/merov-pgdata
PGSOCK=/tmp/merov-pgsock
PORT=5599

"$PGBIN/pg_ctl" -D "$PGDATA" stop -m immediate >/dev/null 2>&1 || true
rm -rf "$PGDATA" "$PGSOCK"
mkdir -p "$PGDATA" "$PGSOCK"

"$PGBIN/initdb" -D "$PGDATA" -U merovingian_test --auth=trust >/dev/null
"$PGBIN/pg_ctl" -D "$PGDATA" -o "-p $PORT -k $PGSOCK -c listen_addresses=127.0.0.1" -l /tmp/merov-pg.log start >/dev/null
sleep 1

export PGHOST=127.0.0.1 PGPORT=$PORT PGUSER=merovingian_test
"$PGBIN/psql" -v ON_ERROR_STOP=1 -d postgres -c "ALTER ROLE merovingian_test PASSWORD 'test-password';" >/dev/null
"$PGBIN/psql" -v ON_ERROR_STOP=1 -d postgres -c "CREATE DATABASE merovingian_test OWNER merovingian_test;" >/dev/null

export PGDATABASE=merovingian_test
"$PGBIN/psql" -v ON_ERROR_STOP=1 >/dev/null <<'SQL'
  REVOKE CREATE ON SCHEMA public FROM PUBLIC;
  CREATE ROLE merovingian_migration NOLOGIN;
  CREATE ROLE merovingian_runtime NOLOGIN;
  GRANT merovingian_migration TO merovingian_test;
  GRANT merovingian_runtime TO merovingian_test;
  GRANT CREATE, USAGE ON SCHEMA public TO merovingian_migration;
  GRANT USAGE ON SCHEMA public TO merovingian_runtime;
  ALTER DEFAULT PRIVILEGES FOR ROLE merovingian_test IN SCHEMA public
    GRANT SELECT, INSERT, UPDATE, DELETE ON TABLES TO merovingian_runtime;
  ALTER DEFAULT PRIVILEGES FOR ROLE merovingian_test IN SCHEMA public
    GRANT USAGE, SELECT, UPDATE ON SEQUENCES TO merovingian_runtime;
  ALTER DEFAULT PRIVILEGES FOR ROLE merovingian_migration IN SCHEMA public
    GRANT SELECT, INSERT, UPDATE, DELETE ON TABLES TO merovingian_runtime;
  ALTER DEFAULT PRIVILEGES FOR ROLE merovingian_migration IN SCHEMA public
    GRANT USAGE, SELECT, UPDATE ON SEQUENCES TO merovingian_runtime;
SQL

"$PGBIN/psql" -v ON_ERROR_STOP=1 >/dev/null <<'SQL'
  SELECT format('ALTER TABLE public.%I OWNER TO %I', tablename, 'merovingian_migration')
    FROM pg_tables WHERE schemaname = 'public' AND tableowner = 'merovingian_test'
  \gexec
  SELECT format('ALTER SEQUENCE public.%I OWNER TO %I', sequencename, 'merovingian_migration')
    FROM pg_sequences WHERE schemaname = 'public' AND sequenceowner = 'merovingian_test'
  \gexec
SQL

echo "cluster ready on port $PORT"
