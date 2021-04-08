rmdir /S /Q test
mkdir test
mkdir test\Foo
mkdir test\Bar\1
mkdir test\Bar\2
mkdir test\Bar\3
mkdir test\Bar
mkdir test\Baz\Foo
mkdir test\Baz\Bar

Release\mkbin test\file1.dat --size 10485760
Release\mkbin test\file2.dat --size 10485760
Release\mkbin test\file3.dat --size 10485760
Release\mkbin test\file4.dat --size 10485760
Release\mkbin test\file5.dat --size 10485760
Release\mkbin test\Bar\1\file11.dat --size 10485760
Release\mkbin test\Bar\1\file12.dat --size 10485760
Release\mkbin test\Bar\1\file13.dat --size 10485760
Release\mkbin test\Bar\2\file14.dat --size 10485760
Release\mkbin test\Bar\2\file15.dat --size 10485760
Release\mkbin test\Bar\2\file16.dat --size 10485760
Release\mkbin test\Bar\3\file17.dat --size 10485760
Release\mkbin test\Bar\3\file18.dat --size 10485760
Release\mkbin test\Bar\3\file19.dat --size 10485760
Release\mkbin test\Baz\file21.dat --size 20971520
Release\mkbin test\Baz\file22.dat --size 20971520
Release\mkbin test\Baz\file23.dat --size 20971520
Release\mkbin test\Baz\file24.dat --size 20971520
Release\mkbin test\Baz\file25.dat --size 20971520
Release\mkbin test\Baz\Foo\file26.dat --size 10485760
Release\mkbin test\Baz\Foo\file27.dat --size 10485760
Release\mkbin test\Baz\Foo\file28.dat --size 10485760
Release\mkbin test\Baz\Bar\file31.dat --size 10485760
Release\mkbin test\Baz\Bar\file32.dat --size 10485760
Release\mkbin test\Baz\Bar\file33.dat --size 10485760
Release\mkbin test\Baz\Bar\file34.dat --size 10485760
Release\mkbin test\Baz\Bar\file35.dat --size 10485728

