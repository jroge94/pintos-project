# -*- perl -*-
use strict;
use warnings;
use tests::tests;

our %args;

# Test name and description
$args{'name'} = 'seek-normal';
$args{'description'} = 'Tests the \'seek\' system call';

# Program to run
$args{'test_program'} = 'seek-normal';

# Expected output
$args{'stdout'} = <<'END_OUTPUT';
File content: Hello, World!
seek-normal: pass
END_OUTPUT
pass;
