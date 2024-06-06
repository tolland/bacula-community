# Tests for kubernetes plugin

## Test-0001 PVC Data general tests

### E01 With one pvc

### E02 With two pvc


### B01 Standard backup mode with only one pvc

### B02 Standard backup with two pvcs

### B03 Standard backup mode with two pvcs in different namespaces

### B04 Test standard backup with two pvcs but one of them is in other namespace is not specificated in fileset

### B05 Avoid pvcs which are in status `Terminating`.

### B06 Change mode by incompatibility of clone to standard

Note: When the plugin changes the mode in pvc backup:

 - The plugin creates two files with the same pvc file `.tar`. This is because when try clone is empty and retry again.


### R01 One pvc and pod (From B01)

### R02 Two pvcs and pod (From B02)



## Test-0002

This test is created based on ticket: https://bugs.baculasystems.com/view.php?id=10901

### B01 Cluster IPs

We had an issue with cluster IPs when we restore services with ClusterIPs.


### R01 Service with clusterIP



## Test-0003

These tests are involved to backup PostgreSQL containers with their data.
 
### B01 Pod that it contains postgres container


### R01 Pod with postgres image (From B01)

Note: We create a new table and values inside postgres database. We check if this restore creates a new data because the pvc is "empty" or not.
With the add feature in 2.3.0, we restore pvcdata before pods. So this problem mustn't be happen.



## Test-0004

These tests are involved in different backup modes (snapshot, clone and standard)

### B01 One compatible pvc with specific backup mode: snapshot

### B02 Two compatible pvc with specific backup mode: snapshot

### B03 One compatible pvc with specific backup mode: clone

### B04 Two compatible pvcs with specific backup mode: clone

### B05 One pvc with specific backup mode: standard

### B06 Two pvcs with specific backup mode: standard

### B07 One no-compatible pvc with specific backup mode: snapshot

### B08 One no-compatible pvc with specific backup mode: clone

### B09 Two pvc (one compatible and other not) with specific backup mode: snapshot

### B10 Two pvc (one no-compatible and other yes) with specific backup mode: snapshot

### B11 Two pvc (one compatible and other not) with specific backup mode: clone

### B12 Two pvc (one no-compatible and other yes) with specific backup mode: clone

### B13 One pod with two attached pvcs



### R01 One pvc with snapshot mode (From B01)

### R02 One pvc with clone mode (From B03)

### R03 One pvc with standard mode (From B05)

### R04 One pvc with change mode (From B07)

### R05 Two pvc in same pod (From B13)