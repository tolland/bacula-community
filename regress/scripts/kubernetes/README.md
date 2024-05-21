# Tests for kubernetes plugin

## Test-0001 PVC Data tests

Variables to replace:
```
@K8S_NAMESPACE_2@ = Determine the second namespace to backup
```

### 01 Test standard backup with only one pvc

Specific Variables to replace:
```
@K8S_NAMESPACE_1@ = Determine one namespace to backup
@PVC_0001_1@ = Determine the specific pvc to backup
```

### 02 Test standard backup with two pvcs

Specific variables to replace:
```
@K8S_NAMESPACE_1@ = Determine one namespace to backup
@PVC_0001_1@ = Determine the specific pvc to backup
@PVC_0001_2@ = Determine the specific pvc to backup
```

### 03 Test standard backup with two pvcs in different namespaces

Specific variables to replace:
```
@K8S_NAMESPACE_1@ = Determine one namespace to backup
@K8S_NAMESPACE_2@ = Determine other namespace to backup
@PVC_0001_1@ = Determine the specific pvc in namespace 1 to backup
@PVC_0001_3@ = Determine the specific pvc in namespace 2 to backup
```

### 04 Test standard backup with two pvcs but one of them is in other namespace is not specificated in fileset
 TODO
Specific variables to replace:
```
@K8S_NAMESPACE_1@ = Determine one namespace to backup
@K8S_NAMESPACE_2@ = Determine other namespace to backup
@PVC_0001_1@ = Determine the specific pvc in namespace 1 to backup
@PVC_0001_3@ = Determine the specific pvc in namespace 2 to backup
```

### 05 Test the feature where avoid pvcs which are in status `Terminating`.

Specific Variables to replace:
```
@K8S_NAMESPACE_1@ = Determine one namespace to backup
@PVC_0001_1@ = Determine the specific pvc to backup will be Terminating status
@PVC_N1_0001_2@ = Determine the specific pvc to backup
```


### 06 Test the change mode of clone to standard

Note: When the plugin changes the mode in pvc backup:

 - The plugin creates two files with the same pvc file `.tar`. This is because when try clone is empty and retry again.

Specific variables to replace:
```
@K8S_NAMESPACE_1@ = Determine one namespace to backup
@PVC_0001_1@ = Determine the specific pvc to backup
```

## Test-0002

This tests is created based on ticket: https://bugs.baculasystems.com/view.php?id=10901

### 01 Cluster IPs
