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

use Baculum\Common\Modules\Errors\GenericError;

/**
 * List jobdefs resource names.
 * NOTE: This endpoint results do not use ACLs.
 *
 * @author Marcin Haba <marcin.haba@bacula.pl>
 * @category API
 * @package Baculum API
 */
class JobDefsResNames extends BaculumAPIServer {

	public function get() {
		$component_type = 'dir';
		$resource_type = 'JobDefs';

		// Role valid. Access granted
		$config = $this->getModule('bacula_setting')->getConfig(
			$component_type,
			$resource_type
		);
		if ($config['exitcode'] === 0) {
			$jobdefs = [];
			for ($i = 0; $i < count($config['output']); $i++) {
				$jobdefs[] = $config['output'][$i]['JobDefs']['Name'];
			}
			sort($jobdefs);
			$this->output = $jobdefs;
			$this->error = GenericError::ERROR_NO_ERRORS;
		} else {
			$this->output = GenericError::MSG_ERROR_WRONG_EXITCODE . ' Exitcode=>' . $config['exitcode'];
			$this->error = GenericError::ERROR_WRONG_EXITCODE;
		}
	}
}
