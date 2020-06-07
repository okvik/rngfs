#include <u.h>
#include <libc.h>
#include <fcall.h>
#include <thread.h>
#include <9p.h>

void
usage(void)
{
	fprint(2, "%s [-D] [-m mtpt] [-s srv]\n", argv0);
	exits("usage");
}

enum {
	Qroot,
		Qinteger,
		Qreal,
};

typedef struct F {
	char *name;
	Qid qid;
	ulong mode;
} F;

typedef struct Fstate {
	/* range [min, max) */
	union {long i; double d;} min;
	union {long i; double d;} max;
} Fstate;

F root = {
	"/", {Qroot, 0, QTDIR}, 0555|DMDIR
};
F roottab[] = {
	"integer", {Qinteger, 0, QTFILE}, 0444,
	"real", {Qreal, 0, QTFILE}, 0444,
};

enum {Cmdrange};
Cmdtab cmd[] = {
	Cmdrange, "range", 3,
};

F*
filebypath(uvlong path)
{
	int i;

	if(path == Qroot)
		return &root;
	for(i = 0; i < nelem(roottab); i++)
		if(path == roottab[i].qid.path)
			return &roottab[i];
	return nil;
}

void
xattach(Req *r)
{
	r->fid->qid = filebypath(Qroot)->qid;
	r->ofcall.qid = r->fid->qid;
	respond(r, nil);
}

char*
xwalk1(Fid *fid, char *name, Qid *qid)
{
	int i;
	
	switch(fid->qid.path){
	case Qroot:
		for(i = 0; i < nelem(roottab); i++){
			if(strcmp(roottab[i].name, name) == 0){
				*qid = roottab[i].qid;
				fid->qid = *qid;
				return nil;
			}
		}
		if(strcmp("..", name) == 0){
			*qid = root.qid;
			fid->qid = *qid;
			return nil;
		}
		break;
	}
	return "not found";
}

void
xopen(Req *r)
{
	uvlong path = r->fid->qid.path;
	Fstate *st;

	st = emalloc9p(sizeof(Fstate));
	r->fid->aux = st;
	switch(path){
	case Qinteger:
		st->min.i = 0L;
		st->max.i = 0x7fffffffL;
		break;
	case Qreal:
		st->min.d = 0.0;
		st->max.d = 1.0;
		break;
	}
	respond(r, nil);
}

void
xdestroyfid(Fid *fid)
{
	if(fid->aux != nil)
		free(fid->aux);
}

void
fillstat(Dir *d, uvlong path, char *user)
{
	F *f;

	f = filebypath(path);
	d->name = estrdup9p(f->name);
	d->qid = f->qid;
	d->mode = f->mode;
	d->uid = estrdup9p(user);
	d->gid = estrdup9p(user);
	d->muid = estrdup9p(user);
	d->length = 0;
	d->atime = time(0);
	d->mtime = time(0);
}

void
xstat(Req *r)
{
	fillstat(&r->d, r->fid->qid.path, r->fid->uid);
	respond(r, nil);
	return;
}

int
rootgen(int n, Dir *d, void *aux)
{
	Req *r = aux;

	if(n >= nelem(roottab))
		return -1;
	fillstat(d, roottab[n].qid.path, r->fid->uid);
	return 0;
}

long
randominteger(long min, long max)
{
	if(min > max){int t = min; min = max; max = t;}
	return min+((max-min)/(double)0x7fffffff*lrand());
}

double
randomreal(double min, double max)
{
	if(min > max){double t = min; min = max; max = t;}
	return min+(max-min)*frand();
}

void
xread(Req *r)
{
	char buf[128];
	int n = 0;
	uvlong path = r->fid->qid.path;
	Fstate *fs = r->fid->aux;

	if(path == Qroot){
		dirread9p(r, rootgen, r);
		respond(r, nil);
		return;
	}

	switch(path){
	case Qinteger:
		n = snprint(buf, sizeof buf, "%ld\n",
			randominteger(fs->min.i, fs->max.i));
		break;
	case Qreal:
		n = snprint(buf, sizeof buf, "%f\n",
			randomreal(fs->min.d, fs->max.d));
		break;
	}
	if(r->ifcall.count < n)
		n = r->ifcall.count;
	r->ofcall.count = n;
	memmove(r->ofcall.data, buf, n);
	respond(r, nil);
}

void
xwrite(Req *r)
{
	uvlong path = r->fid->qid.path;
	Cmdbuf *cb;
	Cmdtab *cp;
	Fstate *fs = r->fid->aux;

	cb = parsecmd(r->ifcall.data, r->ifcall.count);
	cp = lookupcmd(cb, cmd, nelem(cmd));
	if(cp == nil){
		respondcmderror(r, cb, "%r");
		return;
	}
	switch(cp->index){
	case Cmdrange:
		switch(path){
		case Qinteger:
			fs->min.i = strtol(cb->f[1], nil, 10);
			fs->max.i = strtol(cb->f[2], nil, 10);
			break;
		case Qreal:
			fs->min.d = strtod(cb->f[1], nil);
			fs->max.d = strtod(cb->f[1], nil);
		}
		break;
	}
	respond(r, nil);
}

Srv fileserver = {
	.attach = xattach,
	.walk1 = xwalk1,
	.open = xopen,
	.stat = xstat,
	.read = xread,
	.write = xwrite,
	
	.destroyfid = xdestroyfid,
};

void
main(int argc, char *argv[])
{
	char *mtpt, *srvn;

	mtpt = "/mnt/random";
	srvn = nil;
	ARGBEGIN{
	case 'm':
		mtpt = EARGF(usage());
		break;
	case 's':
		srvn = EARGF(usage());
		break;
	case 'D':
		chatty9p++;
		break;
	default:
		usage();
	}ARGEND;
	
	srand(time(0));
	postmountsrv(&fileserver, srvn, mtpt, MREPL);
	exits(nil);
}
