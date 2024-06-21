<?php
/*
 * Bacula(R) - The Network Backup Solution
 * Baculum   - Bacula web interface
 *
 * Copyright (C) 2013-2024 Kern Sibbald
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

use Baculum\API\Modules\BaculumAPIServer;
use Baculum\Common\Modules\Errors\StorageError;

/**
 * Disable Director Storage resource endpoint.
 *
 * @author Marcin Haba <marcin.haba@bacula.pl>
 * @category API
 * @package Baculum API
 */
class DisableStorage extends BaculumAPIServer {

	public function set($id, $params) {
		$storageid = (int) $id;
		$drive = $this->Request->contains('drive') ? (int) $this->Request['drive'] : 0;
		$storage_mod = $this->getModule('storage');
		$storage = null;
		if ($storageid > 0) {
			$storage = $storage_mod->getStorageById($storageid);
		}

		$result = $this->getModule('bconsole')->bconsoleCommand(
			$this->director,
			['.storage'],
			null,
			true
		);
		if ($result->exitcode === 0) {
			if (is_object($storage) && in_array($storage->name, $result->output)) {
				$component_type = 'dir';
				$resource_type = 'storage';
				$resource_name = $storage->name;
				$params = ["drive=\"$drive\""];

				$result = $this->getModule('disable')->disable(
					$this->director,
					$component_type,
					$resource_type,
					$resource_name,
					$params
				);
				$output = $result->output;
				$error = $result->exitcode;
				if ($error != 0) {
					$output = StorageError::MSG_ERROR_WRONG_EXITCODE . 'Exitcode=>' . $result->exitcode. ', Output=>' . print_r($result->output, true);
					$error = StorageError::ERROR_WRONG_EXITCODE;
				}
				$this->output = $output;
				$this->error = $error;
			} else {
				$this->output = StorageError::MSG_ERROR_STORAGE_DOES_NOT_EXISTS;
				$this->error = StorageError::ERROR_STORAGE_DOES_NOT_EXISTS;
			}
		} else {
			$this->output = StorageError::MSG_ERROR_WRONG_EXITCODE . 'Exitcode=>' . $result->exitcode. ', Output=>' . print_r($result->output, true);
			$this->error = StorageError::ERROR_WRONG_EXITCODE;
		}
	}
}
?>
