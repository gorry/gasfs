echo 事前にCreateTestFile.batを実行しておくこと。
mkdir testout
Release\MkGasFs.exe test.gfi --output testout\testout --basedir test --list testout.gfi --verbose --force
Release\MkGasFs.exe test.gfi --output testout\testout --basedir test --list testout.gfi --verbose
mkdir testext
Release\ExGasFs.exe testout\testout_000.gfs --extract testext --list testext.gfi --verbose


