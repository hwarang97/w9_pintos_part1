# -*- perl -*-
use strict;
use warnings;
use tests::tests;
check_expected ([<<'EOF']);
exit-syscall: exit(57)
EOF
pass;
