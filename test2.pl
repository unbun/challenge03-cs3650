#!/usr/bin/perl
use 5.16.0;
use warnings FATAL => 'all';

use Test::Simple tests => 42;
use IO::Handle;
use Data::Dumper;

sub mount {
    system("(make mount 2>> error.log) >> test.log &");
    sleep 1;
}

sub unmount {
    system("(make unmount 2>> error.log) >> test.log");
}

sub write_text {
    my ($name, $data) = @_;
    open my $fh, ">", "mnt/$name" or return;
    $fh->say($data);
    close $fh;
}

sub read_text {
    my ($name) = @_;
    open my $fh, "<", "mnt/$name" or return "";
    local $/ = undef;
    my $data = <$fh> || "";
    close $fh;
    $data =~ s/\s*$//;
    return $data;
}

sub read_text_slice {
    my ($name, $count, $offset) = @_;
    open my $fh, "<", "mnt/$name" or return "";
    my $data;
    seek $fh, $offset, 0;
    read $fh, $data, $count;
    close $fh;
    return $data;
}

sub write_text_off {
    my ($name, $offset, $data) = @_;
    open my $fh, "+<", "mnt/$name" or return "";
    seek $fh, $offset, 0;
    syswrite $fh, $data;
    close $fh;
}

mount();

my $huge0 = "=This string is fourty characters long.=" x 1000;
write_text("40k.txt", $huge0);
# my $huge1 = read_text("40k.txt");

# say "# =====================================";
# say "# '$huge0'";
# say "# =====================================";
# say "# '$huge1'";
# say "# =====================================";

# my $nn = `cat mnt/40k.txt | wc -l`;
# ok($nn == 50, "check size of 40k");
# say "40k = $nn";

# ok($huge0 eq $huge1, "Read back 40k correctly.");