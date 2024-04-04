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

namespace Baculum\API\Modules;

use PDO;

/**
 * FileSet manager module.
 *
 * @author Marcin Haba <marcin.haba@bacula.pl>
 * @category Module
 * @package Baculum API
 */
class FileSetManager extends APIModule {

	public function getFileSets($criteria, $limit_val = 0, $offset_val = 0, $sort = [['FileSet', 'ASC']], $unique_vals = false) {
		$order = '';
		if (count($sort) == 1) {
			$order = Database::getOrder($sort);
		}
		$limit = '';
		if (is_int($limit_val) && $limit_val > 0) {
			$limit = ' LIMIT ' . $limit_val;
		}
		$offset = '';
		if (is_int($offset_val) && $offset_val > 0) {
			$offset = ' OFFSET ' . $offset_val;
		}
		$where = Database::getWhere($criteria);
		if ($unique_vals) {
			$sql = '
SELECT
	fs.FileSetId AS filesetid,
	fs.FileSet AS fileset,
	fs.Md5 AS md5,
	fs.CreateTime AS createtime,
	fs.Content AS content
FROM (
	SELECT
		FileSet.*,
		ROW_NUMBER() OVER (PARTITION BY FileSet ORDER BY CreateTime DESC) AS pos
	FROM FileSet
	' . $where['where'] . '
) AS fs
WHERE fs.pos=1 '
. $order . $limit . $offset;
		} else {
			$sql = 'SELECT FileSet.* 
FROM FileSet '
 . $where['where'] . $order . $limit . $offset;
		}

		$statement = Database::runQuery($sql, $where['params']);
		return $statement->fetchAll(PDO::FETCH_OBJ);
	}

	public function getFileSetById($id) {
		return FileSetRecord::finder()->findByfilesetid($id);
	}

	public function getFileSetByName($name) {
		return FileSetRecord::finder()->findByfileset($name);
	}
}
?>
