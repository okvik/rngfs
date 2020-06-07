</$objtype/mkfile

BIN=/bin
TARG=rngfs
OFILES=rngfs.$O

all:V:

test:V:
	rngfs
	<>[3]/mnt/random/integer {
			echo range 5 10 >[1=3]
			<[0=3] sed 999q | awk '$1 < 5 || $1 > 10 {exit $1}'
	}
	<>[3]/mnt/random/real {
			echo range 5.351115 10.5215 >[1=3]
			<[0=3] sed 999q | awk '$1 < 5.351115 || $1 > 10.5215 {exit $1}'
	}

</sys/src/cmd/mkone
