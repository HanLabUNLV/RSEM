
import sys

IDfile=sys.argv[1]
fastqfile = sys.argv[2]

IDs = {} 
inf = open(IDfile,'r')
for line in inf:
  IDs[line.rstrip('\n')] = 1
inf.close()

fqfile = open(fastqfile, 'r')
for line in fqfile:
  cur_line = line.rstrip('\n')
  if cur_line in IDs:
    print cur_line
    print fqfile.next().rstrip('\n') 
    print fqfile.next().rstrip('\n') 
    print fqfile.next().rstrip('\n') 
    IDs.pop(cur_line, None)
    if len(IDs) == 0:
      break;

