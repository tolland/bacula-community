<?php
/*
 * Bacula(R) - The Network Backup Solution
 * Baculum   - Bacula web interface
 *
 * Copyright (C) 2013-2020 Kern Sibbald
 *
 * The main author of Baculum is Marcin Haba.
 * The original author of Bacula is Kern Sibbald, with contributions
 * from many others, a complete list can be found in the file AUTHORS.
 *
 * You may use this file and others of this release according to the
 * license defined in the LICENSE file, which includes the Affero General
 * Public License, v3.0 ("AGPLv3") and some additional permissions and
 * terms pursuant to its AGPLv3 Section 7.
 *
 * This notice must be preserved when any source code is
 * conveyed and/or propagated.
 *
 * Bacula(R) is a registered trademark of Kern Sibbald.
 */

namespace Baculum\Common\Modules;

use Prado\TModule;

/**
 * Module with miscellaneous tools.
 * Targetly it is meant to remove after splitting into smaller modules.
 *
 * @author Marcin Haba <marcin.haba@bacula.pl>
 * @category Module
 * @package Baculum Common
 */
class Miscellaneous extends TModule {

	const RPATH_PATTERN = '/^b2\d+$/';

	/**
	 * Order directions.
	 */
	const ORDER_DIRECTION_ASC = 'asc';
	const ORDER_DIRECTION_DESC = 'desc';

	public $job_types = array(
		'B' => 'Backup',
		'M' => 'Migrated',
		'V' => 'Verify',
		'R' => 'Restore',
		'I' => 'Internal',
		'D' => 'Admin',
		'A' => 'Archive',
		'C' => 'Copy',
		'c' => 'Copy Job',
		'g' => 'Migration'
	);

	private $jobLevels = array(
		'F' => 'Full',
		'I' => 'Incremental',
		'D' => 'Differential',
		'B' => 'Base',
		'f' => 'VirtualFull',
		'V' => 'InitCatalog',
		'C' => 'Catalog',
		'O' => 'VolumeToCatalog',
		'd' => 'DiskToCatalog',
		'A' => 'Data'
	);

	public $backupJobLevels = ['F', 'I', 'D'];

	public $jobStates =  array(
		'C' => array('value' => 'Created', 'description' =>'Created but not yet running'),
		'R' => array('value' => 'Running', 'description' => 'Running'),
		'B' => array('value' => 'Blocked', 'description' => 'Blocked'),
		'T' => array('value' => 'Terminated', 'description' =>'Terminated normally'),
		'W' => array('value' => 'Terminated', 'description' =>'Terminated normally with warnings'),
		'E' => array('value' => 'Error', 'description' =>'Terminated in Error'),
		'e' => array('value' => 'Non-fatal error', 'description' =>'Non-fatal error'),
		'f' => array('value' => 'Fatal error', 'description' =>'Fatal error'),
		'D' => array('value' => 'Verify Diff.', 'description' =>'Verify Differences'),
		'A' => array('value' => 'Canceled', 'description' =>'Canceled by the user'),
		'I' => array('value' => 'Incomplete', 'description' =>'Incomplete Job'),
		'F' => array('value' => 'Waiting on FD', 'description' =>'Waiting on the File daemon'),
		'S' => array('value' => 'Waiting on SD', 'description' =>'Waiting on the Storage daemon'),
		'm' => array('value' => 'Waiting for new vol.', 'description' =>'Waiting for a new Volume to be mounted'),
		'M' => array('value' => 'Waiting for mount', 'description' =>'Waiting for a Mount'),
		's' => array('value' => 'Waiting for storage', 'description' =>'Waiting for Storage resource'),
		'j' => array('value' => 'Waiting for job', 'description' =>'Waiting for Job resource'),
		'c' => array('value' => 'Waiting for client', 'description' =>'Waiting for Client resource'),
		'd' => array('value' => 'Waiting for Max. jobs', 'description' =>'Wating for Maximum jobs'),
		't' => array('value' => 'Waiting for start', 'description' =>'Waiting for Start Time'),
		'p' => array('value' => 'Waiting for higher priority', 'description' =>'Waiting for higher priority job to finish'),
		'i' => array('value' => 'Batch insert', 'description' =>'Doing batch insert file records'),
		'a' => array('value' => 'Despooling attributes', 'description' =>'SD despooling attributes'),
		'l' => array('value' => 'Data despooling', 'description' =>'Doing data despooling'),
		'L' => array('value' => 'Commiting data', 'description' =>'Committing data (last despool)')
	);

	private $jobStatesOK = array('T', 'D');
	private $jobStatesWarning = array('W');
	private $jobStatesError = array('E', 'e', 'f', 'I');
	private $jobStatesCancel = array('A');
	private $jobStatesRunning = array('C', 'R', 'B', 'F', 'S', 'm', 'M', 's', 'j', 'c', 'd','t', 'p', 'i', 'a', 'l', 'L');

	private $runningJobStates = array('C', 'R');

	private $components = array(
		'dir' => array(
			'full_name' => 'Director',
			'url_name' => 'director',
			'main_resource' => 'Director'
		),
		'sd' => array(
			'full_name' => 'Storage Daemon',
			'url_name' => 'storage',
			'main_resource' => 'Storage'
		),
		'fd' => array(
			'full_name' => 'File Daemon',
			'url_name' => 'client',
			'main_resource' => 'FileDaemon'
		),
		'bcons' => array(
			'full_name' => 'Console',
			'url_name' => 'console',
			'main_resource' => 'Director'
		)
	);

	private $replace_opts = array(
		'always',
		'ifnewer',
		'ifolder',
		'never'
	);

	public function getJobLevels() {
		return $this->jobLevels;
	}

	public function getJobState($jobStateLetter = null) {
		$state = '';
		if(is_null($jobStateLetter)) {
			$state = $this->jobStates;
		} else {
			$state = array_key_exists($jobStateLetter, $this->jobStates) ? $this->jobStates[$jobStateLetter] : null;
		}
		return $state;
	}

	public function isJobRunning($jobstatus) {
		$running_job_states = $this->getRunningJobStates();
		return in_array($jobstatus, $running_job_states);
	}

	public function getRunningJobStates() {
		return $this->runningJobStates;
	}

	public function getComponents() {
		return array_keys($this->components);
	}

	public function getMainComponentResource($type) {
		$resource = null;
		if (array_key_exists($type, $this->components)) {
			$resource = $this->components[$type]['main_resource'];
		}
		return $resource;
	}

	public function getComponentFullName($type) {
		$name = '';
		if (array_key_exists($type, $this->components)) {
			$name = $this->components[$type]['full_name'];
		}
		return $name;
	}

	public function getComponentUrlName($type) {
		$name = '';
		if (key_exists($type, $this->components)) {
			$name = $this->components[$type]['url_name'];
		}
		return $name;
	}

	public function getJobStatesByType($type) {
		$statesByType = array();
		$states = array();
		switch($type) {
			case 'ok':
				$states = $this->jobStatesOK;
				break;
			case 'warning':
				$states = $this->jobStatesWarning;
				break;
			case 'error':
				$states = $this->jobStatesError;
				break;
			case 'cancel':
				$states = $this->jobStatesCancel;
				break;
			case 'running':
				$states = $this->jobStatesRunning;
				break;
		}

		for ($i = 0; $i < count($states); $i++) {
			$statesByType[$states[$i]] = $this->getJobState($states[$i]);
		}

		return $statesByType;
	}

	/*
	 * @TODO: Move it to separate validation module.
	 */
	public function isValidJobLevel($jobLevel) {
		return key_exists($jobLevel, $this->getJobLevels());
	}

	public function isValidJobType($job_type) {
		return key_exists($job_type, $this->job_types);
	}

	/**
	 * Validate more than one job type.
	 *
	 * @param string $job_types job types ex. 'BCg'
	 * @return true if all provided job types are valid, otherwise false
	 */
	public function areValidJobTypes($job_types) {
		$types = str_split($job_types);
		$valid = true;
		for ($i = 0; $i < count($types); $i++) {
			if (!$this->isValidJobType($types[$i])) {
				$valid = false;
				break;
			}
		}
		return $valid;
	}

	public function isValidName($name) {
		return (preg_match('/^[\w:\.\-\s]{1,127}$/', $name) === 1);
	}

	public function isValidNameExt($name_ext) {
		return (preg_match('/^[\w:\.\-\s\*=@:]{1,127}$/', $name_ext) === 1);
	}

	public function isValidState($state) {
		return (preg_match('/^[\w\-]+$/', $state) === 1);
	}

	public function isValidInteger($num) {
		return (preg_match('/^\d+$/', $num) === 1);
	}

	public function isValidBoolean($val) {
		return (preg_match('/^(yes|no|1|0|true|false)$/i', $val) === 1);
	}

	public function isValidBooleanTrue($val) {
		return (preg_match('/^(yes|1|true)$/i', $val) === 1);
	}

	public function isValidBooleanFalse($val) {
		return (preg_match('/^(no|0|false)$/i', $val) === 1);
	}

	public function isValidId($id) {
		return ((preg_match('/^\d+$/', $id) === 1) && $id > 0);
	}

	public function isValidPath($path) {
		return (preg_match('/^[\p{L}\p{N}\p{Z}\p{Sc}\p{Pd}\[\]\-\'\/\\(){}:.#~_,+!$]{0,10000}$/u', $path) === 1);
	}

	public function isValidFilename($path) {
		return (preg_match('/^[\p{L}\p{N}\p{Z}\p{Sc}\p{Pd}\[\]\-\'\\(){}:.#~_,+!$]{0,1000}$/u', $path) === 1);
	}

	public function isValidReplace($replace) {
		return in_array($replace, $this->replace_opts);
	}

	public function isValidIdsList($list) {
		return (preg_match('/^[\d,]+$/', $list) === 1);
	}

	public function isValidBvfsPath($path) {
		return (preg_match('/^b2\d+$/', $path) === 1);
	}

	public function isValidBDate($date) {
		return (preg_match('/^\d{4}-\d{2}-\d{2}$/', $date) === 1);
	}

	public function isValidBDateAndTime($time) {
		return (preg_match('/^\d{4}-\d{2}-\d{2} \d{1,2}:\d{2}:\d{2}$/', $time) === 1);
	}

	public function isValidRange($range) {
		return (preg_match('/^[\d\-\,]+$/', $range) === 1);
	}

	public function isValidAlphaNumeric($str) {
		return (preg_match('/^[a-zA-Z0-9]+$/', $str) === 1);
	}

	public function isValidListFilesType($type) {
		return (preg_match('/^(all|deleted)$/', $type) === 1);
	}

	public function isValidOutput($type) {
		return (preg_match('/^(raw|json)$/', $type) === 1);
	}

	public function isValidUUID($uuid) {
		return (preg_match('/^[\w]{8}(-[\w]{4}){3}-[\w]{12}$/', $uuid) === 1);
	}

	public function isValidEmail($email) {
		return filter_var($email, FILTER_VALIDATE_EMAIL);
	}

	public function isValidColumn($column) {
		return (preg_match('/^[\w+.]+$/i', $column) === 1);
	}
	public function isValidOrderDirection($order) {
		return (preg_match('/^(asc|desc)$/i', $order) === 1);
	}

	public function isValidResultView($view) {
		return (preg_match('/^(basic|full|advanced)$/', $view) === 1);
	}

	public function isValidVolType($voltype) {
		$voltypes = ['disk', 'tape', 'cloud'];
		return in_array($voltype, $voltypes);
	}

	public function escapeCharsToConsole($path) {
		return preg_replace('/([$])/', '\\\${1}', $path);
	}

	public function objectToArray($data) {
		return json_decode(json_encode($data), true);
	}

	public function findJobIdStartedJob($output) {
		$jobid = null;
		$output = array_reverse($output); // jobid is ussually at the end of output
		for ($i = 0; $i < count($output); $i++) {
			if (preg_match('/^Job queued\.\sJobId=(?P<jobid>\d+)$/i', $output[$i], $match) === 1) {
				$jobid = $match['jobid'];
				break;
			}
		}
		return $jobid;
	}

	public static function sortResultsByField(&$result, $order_by, $order_direction, $sec_order_by = '', $sec_order_direction = '') {
		$order_by = strtolower($order_by);
		$order_direction = strtolower($order_direction);
		$sec_order_by = strtolower($sec_order_by);
		$sec_order_direction = strtolower($sec_order_direction);
		$sort_by_func = function($a, $b) use ($order_by, $order_direction, $sec_order_by, $sec_order_direction) {
			$cmp = 0;
			if ($a[$order_by] == $b[$order_by]) {
				if ($sec_order_by && $sec_order_direction) {
					if ($sec_order_direction === self::ORDER_DIRECTION_ASC) {
						$cmp = strnatcasecmp($a[$sec_order_by], $b[$sec_order_by]);
					} elseif ($sec_order_direction === self::ORDER_DIRECTION_DESC) {
						$cmp = strnatcasecmp($b[$sec_order_by], $a[$sec_order_by]);
					}
				}
			} else {
				$cmp = strnatcasecmp($a[$order_by], $b[$order_by]);
				if ($order_direction === self::ORDER_DIRECTION_DESC) {
					$cmp = -$cmp;
				}
			}
			return $cmp;
		};
		usort($result, $sort_by_func);
	}
}
?>
