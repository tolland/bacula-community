<?php
/*
 * Bacula(R) - The Network Backup Solution
 * Baculum   - Bacula web interface
 *
 * Copyright (C) 2013-2019 Kern Sibbald
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
use Baculum\Common\Modules\Errors\FileSetError;

/**
 * FileSets endpoint.
 *
 * @author Marcin Haba <marcin.haba@bacula.pl>
 * @category API
 * @package Baculum API
 */
class FileSets extends BaculumAPIServer {

	public function get() {
		$misc = $this->getModule('misc');
		$content = $this->Request->contains('content') && $misc->isValidNameList($this->Request['content']) ? $this->Request['content'] : '';
		$limit = $this->Request->contains('limit') && $misc->isValidInteger($this->Request['limit']) ? (int)$this->Request['limit'] : 0;
		$offset = $this->Request->contains('offset') && $misc->isValidInteger($this->Request['offset']) ? (int)$this->Request['offset'] : 0;
		$unique_filesets = $this->Request->contains('unique_filesets') && $misc->isValidBooleanTrue($this->Request['unique_filesets']) ? true : false;
		$result = $this->getModule('bconsole')->bconsoleCommand(
			$this->director,
			['.fileset']
		);
		if ($result->exitcode === 0) {
			array_shift($result->output);
			$vals = array_filter($result->output);
			if (count($vals) == 0) {
				// no $vals criteria means that user has no fileset resource assigned.
				$this->output = [];
				$this->error = FileSetError::ERROR_NO_ERRORS;
				return;
			}

			$params['FileSet.FileSet'] = [];
			$params['FileSet.FileSet'][] = [
				'operator' => 'IN',
				'vals' => $vals
			];

			if (!empty($content)) {
				$cnts = explode(',', $content);
				$cb = function ($item) {
					return ('%' . trim($item) . '%');
				};
				$cnts = array_map($cb, $cnts);
				$params['FileSet.Content'][] = [
					'operator' => 'LIKE',
					'vals' => $cnts
				];
			}

			$filesets = $this->getModule('fileset')->getFileSets(
				$params,
				$limit,
				$offset,
				$unique_filesets
			);
			$this->output = $filesets;
			$this->error = FileSetError::ERROR_NO_ERRORS;
		} else {
			$this->output = FileSetError::MSG_ERROR_WRONG_EXITCODE . 'ErrorCode=' . $result->exitcode . ' Output=' . implode(PHP_EOL, $result->output);
			$this->error = FileSetError::ERROR_WRONG_EXITCODE;
		}
	}
}

?>
