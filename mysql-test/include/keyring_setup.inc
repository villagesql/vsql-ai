# Restart the server with component_keyring_file and install the keyring_udf
# functions, so a test can store API keys with keyring_key_store().
# Always pair with keyring_teardown.inc.
#
# MTR writes the global manifest and keyring config itself, both pointing at
# the data directory ("read_local_manifest" / "read_local_config"), so the
# component is enabled by writing a local manifest and config there.

--source include/have_component_keyring_file.inc

--disable_query_log
--let CURRENT_DATADIR = `SELECT @@datadir`
--let VSQL_AI_KEYRING_PATH = $MYSQLTEST_VARDIR/vsql_ai_keyring
--perl
  use strict;
  use File::Basename;
  my ($exename) = fileparse($ENV{'MYSQLD'}, qr/\.[^.]*/);
  my $datadir = $ENV{'CURRENT_DATADIR'};
  open(my $m, ">", "$datadir/$exename.my") or die "cannot write manifest";
  print $m '{ "components": "file://component_keyring_file" }';
  close($m);
  open(my $c, ">", "$datadir/component_keyring_file.cnf")
    or die "cannot write keyring config";
  print $c '{ "path": "' . $ENV{'VSQL_AI_KEYRING_PATH'} . '", "read_only": false }';
  close($c);
EOF
--let $restart_parameters = restart:
--source include/restart_mysqld_no_echo.inc

# The restart include turns the query log back on.
--disable_query_log
--eval INSTALL PLUGIN keyring_udf SONAME '$KEYRING_UDF'
--eval CREATE FUNCTION keyring_key_store RETURNS INTEGER SONAME '$KEYRING_UDF'
--eval CREATE FUNCTION keyring_key_remove RETURNS INTEGER SONAME '$KEYRING_UDF'
--enable_query_log

SELECT STATUS_VALUE AS keyring_status
  FROM performance_schema.keyring_component_status
 WHERE STATUS_KEY = 'Component_status';
