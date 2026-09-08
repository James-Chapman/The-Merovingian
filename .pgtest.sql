PREPARE p1 AS SELECT pg_advisory_lock($1);
PREPARE p2 AS SELECT pg_advisory_lock($1::bigint);
EXECUTE p2(1234);
