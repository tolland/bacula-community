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

use Baculum\Common\Modules\Errors\AWSToolError;
use Baculum\API\Modules\BaculumAPIServer;

/**
 * AWS cloud list buckets.
 *
 * @author Marcin Haba <marcin.haba@bacula.pl>
 * @category API
 * @package Baculum API
 */
class AWSListBuckets extends BaculumAPIServer {

	public function get() {
		$misc = $this->getModule('misc');
		$access_key = $this->Request->contains('access_key') && $misc->isValidLogin($this->Request['access_key']) ? $this->Request['access_key'] : '';
		$secret_key = $this->Request->contains('secret_key') && $misc->isValidSecret($this->Request['secret_key']) ? $this->Request['secret_key'] : '';
		$endpoint = $this->Request->contains('endpoint') && $misc->isValidURL($this->Request['endpoint']) ? $this->Request['endpoint'] : '';
		$region = $this->Request->contains('region') && $misc->isValidName($this->Request['region']) ? $this->Request['region'] : '';
		if (empty($access_key)) {
			$this->error = AWSToolError::ERROR_AWS_MISSING_ACCESS_KEY;
			$this->output = AWSToolError::MSG_ERROR_AWS_MISSING_ACCESS_KEY;
			return;
		}
		if (empty($secret_key)) {
			$this->error = AWSToolError::ERROR_AWS_MISSING_SECRET_KEY;
			$this->output = AWSToolError::MSG_ERROR_AWS_MISSING_SECRET_KEY;
			return;
		}
		$params['access_key'] = $access_key;
		$params['secret_key'] = $secret_key;
		$params['options'] = [
			's3api',
			'list-buckets'
		];
		if (!empty($endpoint)) {
			 $params['options'][] = '--endpoint-url=' . $endpoint;
		}
		if (!empty($region)) {
			 $params['options'][] = '--region=' . $region;
		}
		$result = $this->getModule('aws_tool')->execCommand($params);
		$this->output = $result['output'];
		$this->error = $result['error'];
	}
}
