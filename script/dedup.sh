FQFILE1=$1
FQFILE2=$2

FQBASE1=${FQFILE1##*/}
FQBASE2=${FQFILE2##*/}
DIRNAME=`dirname $FQFILE1`
echo $FQBASE1
echo $FQBASE2
echo $DIRNAME

awk 'NR % 4 == 1' ${FQFILE1} > ${DIRNAME}/1.IDs &
awk 'NR % 4 == 1' ${FQFILE2} > ${DIRNAME}/2.IDs &

grep '\/2$' ${DIRNAME}/1.IDs > ${DIRNAME}/2in1.IDs
grep '\/1$' ${DIRNAME}/2.IDs > ${DIRNAME}/1in2.IDs

grep -A 3 -F -f ${DIRNAME}/2in1.IDs ${FQFILE1} >> ${FQFILE2}
grep -A 3 -F -f ${DIRNAME}/1in2.IDs ${FQFILE2} >> ${FQFILE1}

grep -v '\/2$' ${DIRNAME}/1.IDs  > ${DIRNAME}/1.clean.IDs
cat ${DIRNAME}/1in2.IDs  >> ${DIRNAME}/1.clean.IDs
grep -v '\/1$' ${DIRNAME}/2.IDs  > ${DIRNAME}/2.clean.IDs
cat ${DIRNAME}/2in1.IDs  >> ${DIRNAME}/2.clean.IDs

sed 's/\//\t/' ${DIRNAME}/1.clean.IDs | sort -k 1,1 |  uniq > ${DIRNAME}/1.uniq.IDs
sed 's/\//\t/' ${DIRNAME}/2.clean.IDs | sort -k 1,1 |  uniq > ${DIRNAME}/2.uniq.IDs

join ${DIRNAME}/1.uniq.IDs ${DIRNAME}/2.uniq.IDs > ${DIRNAME}/common.IDs
awk '{print $1"/1"}' ${DIRNAME}/common.IDs > ${DIRNAME}/1.common.IDs &
pid=$!
awk '{print $1"/2"}' ${DIRNAME}/common.IDs > ${DIRNAME}/2.common.IDs 
wait $pid

split -C 300m --numeric-suffixes ${DIRNAME}/1.common.IDs ${DIRNAME}/split.1.common.IDs. &
pid=$!
split -C 300m --numeric-suffixes ${DIRNAME}/2.common.IDs ${DIRNAME}/split.2.common.IDs.
wait $pid


for file in ${DIRNAME}/split.1.*
do 
  python dedup.py ${file} ${FQFILE1} > ${file}.fastq &
  pids[${i}]=$!
done

for file in ${DIRNAME}/split.2.*
do 
  python dedup.py ${file} ${FQFILE2} > ${file}.fastq &
  pids[${i}]=$!
done

# wait for all pids
for pid in ${pids[*]}; do
    wait $pid
done

cat ${DIRNAME}/split.1.*.fastq > ${FQFILE1}.dedup.fastq
cat ${DIRNAME}/split.2.*.fastq > ${FQFILE2}.dedup.fastq
