echo ���O��CreateTestFile.bat�����s���Ă������ƁB
mkdir testout
Release\MkGasFs.exe test.gfi --output testout\testout --basedir test --list testout.gfi --verbose --force
Release\MkGasFs.exe test.gfi --output testout\testout --basedir test --list testout.gfi --verbose
mkdir testext
Release\ExGasFs.exe testout\testout_000.gfs --extract testext --list testext.gfi --verbose


