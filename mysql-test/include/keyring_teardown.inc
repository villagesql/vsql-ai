# Undo keyring_setup.inc: drop the keyring_udf functions, remove the local
# manifest and keyring config, and have MTR restart the server without a
# keyring before the next test.

--disable_query_log
DROP FUNCTION keyring_key_store;
DROP FUNCTION keyring_key_remove;
UNINSTALL PLUGIN keyring_udf;
--perl
  use strict;
  use File::Basename;
  my ($exename) = fileparse($ENV{'MYSQLD'}, qr/\.[^.]*/);
  my $datadir = $ENV{'CURRENT_DATADIR'};
  unlink "$datadir/$exename.my";
  unlink "$datadir/component_keyring_file.cnf";
  unlink $ENV{'VSQL_AI_KEYRING_PATH'};
EOF
--source include/force_restart.inc
--enable_query_log
