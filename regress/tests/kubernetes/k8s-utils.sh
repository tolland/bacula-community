# /bin/bash

#
# Check a value of parameter if greater and less than input values. 
#
# in:
# $1 - type of log. Values allowed: b,r,e,l
# $2 - the parameter to check. For example: 'FD Bytes Written', 'SD Bytes Written', 'jobbytes'
# $3 - number to use to operation greater than. Can use *KB, *MB format. Example: 5, 50KB, 100MB 
# $4 - number to use to operation less than. Can use *KB, *MB format. Example: 5, 50KB, 100MB 
# $5 - a test number to examine which means we will check log${ltest}.out logfile
check_regress_size_backup() {
   type_log=$1
   param=$2
   gt_value=$3
   lt_value=$4
   n_test=$5

   bytes_written_value=$(get_value_of_parameter_in_log $type_log "${param}" "${n_test}")
   
   check_regress_number_in_log $type_log "gt" "${param}" "${gt_value}" "${n_test}"
   F=$?
   printf "Result expected bytes backed up ;%s; \n" "${F}"
   printf "%s%s\n" " -> Nº bytes of this backup (${gt_value} < ${bytes_written_value}): " $(regress_test_result ${F})
   
   check_regress_number_in_log $type_log "lt" "${param}" "${lt_value}" "${n_test}"
   F=$?
   printf "Result expected bytes backed up ;%s; \n" "${F}"
   printf "%s%s\n" " -> Nº bytes of this backup (${bytes_written_value} < ${lt_value}): " $(regress_test_result ${F})
}

#
# Setup pod annotations in kubernetes
#
# in:
# $1: pod_name
# $2: mode to annotate
# $3: volumes to annotate
set_up_k8s_annotations() {
   pod=$1
   mode_value=$2
   vols_value=$3
   BACKUP_MODE_ANN=bacula/backup.mode
   BACKUP_VOL_ANN=bacula/backup.volumes
   
   # --- SetUp
   ${KUBECTL} annotate pod $pod ${BACKUP_MODE_ANN}=${mode_value} --overwrite > /dev/null
   ${KUBECTL} annotate pod $pod ${BACKUP_VOL_ANN}=${vols_value} --overwrite > /dev/null
}

end_set_up_k8s_annotations() {
   pod=$1
   BACKUP_MODE_ANN=bacula/backup.mode
   BACKUP_VOL_ANN=bacula/backup.volumes

   # Remove annotation in pod.
   ${KUBECTL} annotate pod ${POD_WITH_ANNOTATIONS} ${BACKUP_MODE_ANN}- > /dev/null
   ${KUBECTL} annotate pod ${POD_WITH_ANNOTATIONS} ${BACKUP_VOL_ANN}- > /dev/null
}

wait_until_pod_run() {
   NAMESPACE=$1
   POD=$2
   i=0
   SPIN=('-' '\\' '|' '/')
   printf "\n ... Waiting to pod is running ... \n"
   sleep 3
   while true
   do
      kstat=`${KUBECTL} -n ${NAMESPACE} get pods ${POD} | grep "Running" | wc -l`
      if [ $kstat -eq 1 ]
      then
         break
      fi;
      w=1
      printf "\b${SPIN[(($i % 4))]}"
      if [ $i -eq 60 ]
      then
         echo "Timeout waiting for pod is running. Cannot continue!"
         exit 1
      fi
      ((i++))
      sleep 1
   done
   sleep 3
}