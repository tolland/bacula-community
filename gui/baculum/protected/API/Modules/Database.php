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

use Baculum\Common\Modules\Logging;
use Baculum\Common\Modules\Errors\DatabaseError;
use Baculum\API\Modules\JobRecord;
use Prado\Prado;
use Prado\Data\TDbConnection;
use Prado\Exceptions\TDbException;

/**
 * Database module.
 *
 * @author Marcin Haba <marcin.haba@bacula.pl>
 * @category Database
 * @package Baculum API
 */
class Database extends APIModule {

	public $ID;

	/**
	 * Supported database types
	 */
	const PGSQL_TYPE = 'pgsql';
	const MYSQL_TYPE = 'mysql';
	const SQLITE_TYPE = 'sqlite';

	/**
	 * SQL query builder.
	 *
	 * @var TDbCommandBuilder command builder
	 */
	private static $query_builder;

	/**
	 * Check/test connection to database.
	 * 
	 * @access public
	 * @param array $db_params params to database connection
	 * @return array true if test passed, otherwise false
	 * @throws BCatalogException if problem with connection to database
	 */
	public function testDbConnection(array $db_params) {
		$is_connection = false;
		$tables_format = null;

		try {
			$connection = APIDbModule::getAPIDbConnection($db_params, true);
			$connection->setActive(true);
			$tables_format = $this->getTablesFormat($connection);
			$is_connection = (is_numeric($tables_format) === true && $tables_format > 0);
		} catch (TDbException $e) {
			throw new BCatalogException(
				DatabaseError::MSG_ERROR_DB_CONNECTION_PROBLEM . ' ' . $e->getErrorMessage(),
				DatabaseError::ERROR_DB_CONNECTION_PROBLEM
			);
		}

		if(array_key_exists('password', $db_params)) {
			// mask database password.
			$db_params['password'] = preg_replace('/.{1}/', '*', $db_params['password']);
		}

		$logmsg = 'DBParams=%s, Connection=%s, TablesFormat=%s';
		$msg = sprintf($logmsg, print_r($db_params, true), var_export($is_connection, true), var_export($tables_format, true));
		$this->getModule('logging')->log(
			Logging::CATEGORY_APPLICATION,
			$msg
		);
		return $is_connection;
	}

	/**
	 * Test connection to the Catalog using params from config.
	 *
	 * @access public
	 * @return bool true if connection established, otherwise false
	 */
	public function testCatalog() {
		$result = false;
		$api_config = $this->getModule('api_config')->getConfig();
		if (array_key_exists('db', $api_config)) {
			$result = $this->testDbConnection($api_config['db']);
		} else {
			throw new BCatalogException(
				DatabaseError::MSG_ERROR_DATABASE_ACCESS_NOT_SUPPORTED,
				DatabaseError::ERROR_DATABASE_ACCESS_NOT_SUPPORTED
			);
		}
		return $result;
	}

	/**
	 * Get Catalog database tables format
	 * 
	 * @access private
	 * @param TDBConnection $connection handler to database connection
	 * @return mixed Catalog database tables format or null
	 */
	private function getTablesFormat(TDBConnection $connection) {
		$query = 'SELECT versionid FROM Version';
		$command = $connection->createCommand($query);
		$row = $command->queryRow();
		$tables_format = array_key_exists('versionid', $row) ? $row['versionid'] : null;
		return $tables_format;
	}

	public function getDatabaseSize() {
		$db_params = $this->getModule('api_config')->getConfig('db');

		$connection = APIDbModule::getAPIDbConnection($db_params);
		$connection->setActive(true);
		$pdo = $connection->getPdoInstance();

		$size = 0;
		switch ($db_params['type']) {
			case self::PGSQL_TYPE: {
				$sql = "SELECT pg_database_size('{$db_params['name']}') AS dbsize";
				$result = $pdo->query($sql);
				$row = $result->fetch();
				$size = $row['dbsize'];
				break;
			}
			case self::MYSQL_TYPE: {
				$sql = "SELECT Sum(data_length + index_length) AS dbsize FROM information_schema.tables";
				$result = $pdo->query($sql);
				$row = $result->fetch();
				$size = $row['dbsize'];
				break;
			}
			case self::SQLITE_TYPE: {
				$sql = "PRAGMA page_count";
				$result = $pdo->query($sql);
				$page_count = $result->fetch();
				$sql = "PRAGMA page_size";
				$result = $pdo->query($sql);
				$page_size = $result->fetch();
				$size = ($page_count['page_count'] * $page_size['page_size']);
				break;
			}
		}
		$dbsize = array('dbsize' => $size, 'dbtype' => $db_params['type']);
		$pdo = null;
		return $dbsize;
	}

	public static function getWhere(array $params, $without_where = false) {
		$where = '';
		$parameters = array();
		if (count($params) > 0) {
			$condition = [];
			foreach ($params as $key => $value) {
				for ($i = 0; $i < count($value); $i++) {
					$cond = [];
					$vals = [];
					$kval = str_replace('.', '_', $key);
					if (!isset($value[$i]['operator'])) {
						$value[$i]['operator'] = '';
					}
					if (is_array($value[$i]['vals'])) {
						if ($value[$i]['operator'] == 'IN') {
							// IN operator is treated separately
							$tcond = [];
							for ($j = 0; $j < count($value[$i]['vals']); $j++) {
								$tcond[] = ":{$kval}{$i}{$j}";
								$vals[":{$kval}{$i}{$j}"] = $value[$i]['vals'][$j];
							}
							$cond[] = "{$key} {$value[$i]['operator']} (" . implode(',', $tcond) . ')';
							$value[$i]['operator'] = '';
						} elseif ($value[$i]['operator'] == 'LIKE') {
							$tcond = [];
							for ($j = 0; $j < count($value[$i]['vals']); $j++) {
								$tcond[] = "{$key} {$value[$i]['operator']} :{$kval}{$i}{$j}";
								$vals[":{$kval}{$i}{$j}"] = $value[$i]['vals'][$j];
							}
							$cond[] = implode(' OR ', $tcond);
							$value[$i]['operator'] = '';
						} else {
							// other operators
							for ($j = 0; $j < count($value[$i]['vals']); $j++) {
								$cond[] = "{$key} = :{$kval}{$i}{$j}";
								$vals[":{$kval}{$i}{$j}"] = $value[$i]['vals'][$j];
							}
						}
					} elseif (in_array($value[$i]['operator'], ['>', '<', '>=', '<='])) {
						$cond[] = "{$key} {$value[$i]['operator']} :{$kval}{$i}";
						$vals[":{$kval}{$i}"] = $value[$i]['vals'];
						$value[$i]['operator'] = '';
					} elseif (in_array($value[$i]['operator'], ['IS', 'IS NOT'])) {
						$cond[] = "{$key} {$value[$i]['operator']} {$value[$i]['vals']}";
						$value[$i]['operator'] = '';
					} elseif ($value[$i]['operator'] == 'LIKE') {
						$cond[] = "{$key} {$value[$i]['operator']} :{$kval}{$i}";
						$vals[":{$kval}{$i}"] = $value[$i]['vals'];
						$value[$i]['operator'] = '';
					} elseif (in_array($value[$i]['operator'], ['REGEXP', 'REGEXP BINARY', '~', '~*'])) {
						$cond[] = "{$key} {$value[$i]['operator']} :{$kval}{$i}";
						$vals[":{$kval}{$i}"] = $value[$i]['vals'];
						$value[$i]['operator'] = '';
					} elseif ($value[$i]['operator'] == '=') {
						$cond[] = "$key = :{$kval}{$i}";
						$vals[":{$kval}{$i}"] = $value[$i]['vals'];
						$value[$i]['operator'] = '';
					} elseif ($value[$i]['operator'] == '!=') {
						$cond[] = "$key != :{$kval}{$i}";
						$vals[":{$kval}{$i}"] = $value[$i]['vals'];
						$value[$i]['operator'] = '';
					} else {
						$cond[] = "$key = :{$kval}{$i}";
						$vals[":{$kval}{$i}"] = $value[$i]['vals'];
					}
					$condition[] = implode(' ' . $value[$i]['operator'] . ' ', $cond);
					foreach ($vals as $pkey => $pval) {
						$parameters[$pkey] = $pval;
					}
				}
			}
			if (count($condition) > 0) {
				$where = ' (' . implode(') AND (' , $condition) . ')';
				if ($without_where === false)  {
					$where = ' WHERE ' . $where;
				}
			}
		}
		return array('where' => $where, 'params' => $parameters);
	}

	/**
	 * Group database records by specific column.
	 *
	 * @param string $group_by column to use as group
	 * @param array $result database results/records (please note - reference)
	 * @param integer $group_limit group limit (zero means no limit)
	 * @param integer $group_offset group offset
	 * @param string|null $overview_by prepare overview (counts) by given object output property
	 * @param string|null $sort_by sort group by given object property
	 * @param string $sort_direction sort direction (asc|desc)
	 * @param array overview array or empty array if no overview requested
	 */
	public static function groupBy($group_by, &$result, $group_limit = 0, $group_offset = 0, $overview_by = null, $sort_by = null, $sort_direction = 'asc') {
		$overview = [];
		if (is_string($group_by) && is_array($result)) {
			// Group results
			$new_result = [];
			$len = count($result);
			$group_cnts = [];
			for ($i = 0; $i < $len; $i++) {
				if (!property_exists($result[$i], $group_by)) {
					continue;
				}
				if (!key_exists($result[$i]->{$group_by}, $new_result)) {
					$new_result[$result[$i]->{$group_by}] = [];
				}
				if (!key_exists($result[$i]->{$group_by}, $group_cnts)) {
					$group_cnts[$result[$i]->{$group_by}] = 0;
				}
				if ($group_cnts[$result[$i]->{$group_by}] < $group_offset) {
					$group_cnts[$result[$i]->{$group_by}]++;
					// don't display elements lower than offset
					continue;
				}
				if ($group_limit > 0 && count($new_result[$result[$i]->{$group_by}]) >= $group_limit) {
					// limit per group reached
					continue;
				}
				$group_cnts[$result[$i]->{$group_by}]++;
				$new_result[$result[$i]->{$group_by}][] = $result[$i];
				if (!key_exists($result[$i]->{$overview_by}, $overview)) {
					$overview[$result[$i]->{$overview_by}] = [
						$overview_by => $result[$i]->{$overview_by},
						'count' => 0
					];
				}
				if (is_string($overview_by)) {
					$overview[$result[$i]->{$overview_by}]['count']++;
				}
			}
			$result = $new_result;
			if (is_string($sort_by) && is_string($sort_direction)) {
				$sort_direction = strtolower($sort_direction);
				$func = function ($a, $b) use ($sort_by, $sort_direction) {
					if (!property_exists($a, $sort_by) || !property_exists($b, $sort_by)) {
						return 0;
					}
					$res = strcmp($a->{$sort_by}, $b->{$sort_by});
					if ($sort_direction == 'desc') {
						if ($a->{$sort_by} === $b->{$sort_by}) {
							return 0;
						}
						return ($a->{$sort_by} < $b->{$sort_by}) ? 1 : -1;
					} else {
						return $res;
					}
				};
				foreach ($result as $key => &$val) {
					usort($val, $func);
				}
			}
		}
		return array_values($overview);
	}

	/**
	 * Get the SQL query builder instance.
	 * Note: Singleton
	 *
	 * @param object $record Active Record object
	 * @return TDbCommandBuilder command builder
	 */
	private static function getQueryBuilder() {
		if (is_null(self::$query_builder)) {
			$record = JobRecord::finder();
			$connection = $record->getDbConnection();
			$tableInfo = $record->getRecordGateway()->getRecordTableInfo($record);
			self::$query_builder = $tableInfo->createCommandBuilder($connection);
		}
		return self::$query_builder;
	}

	/**
	 * Run query and get SQL query statement.
	 *
	 * @param string $sql SQL query
	 * @param array $params SQL query params
	 * @return PDO statement.
	 */
	public static function runQuery($sql, $params = []) {
		if (count($params) == 0) {
			/**
			 * Please note that in case no params the TDbCommandBuilder::applyCriterias()
			 * returns empty the PDO statement handler. From this reason here
			 * the query is called directly by PDO.
			 */
			$connection = JobRecord::finder()->getDbConnection();
			$connection->setActive(true);
			$pdo = $connection->getPdoInstance();
			$statement = $pdo->query($sql);

		} else {
			$builder = self::getQueryBuilder();
			$command = $builder->applyCriterias($sql, $params);
			$statement = $command->getPdoStatement();
			$command->query();
		}
		return $statement;
	}

	/**
	 * Execute SQL query.
	 *
	 * @param string $sql SQL query
	 * @param array $params SQL query params
	 * @return PDO statement.
	 */
	public static function execute($sql, $params = []) {
		if (count($params) == 0) {
			/**
			 * Please note that in case no params the TDbCommandBuilder::applyCriterias()
			 * returns empty the PDO statement handler. From this reason here
			 * the query is called directly by PDO.
			 */
			$connection = JobRecord::finder()->getDbConnection();
			$connection->setActive(true);
			$pdo = $connection->getPdoInstance();
			$statement = $pdo->exec($sql);

		} else {
			$builder = self::getQueryBuilder();
			$command = $builder->applyCriterias($sql, $params);
			$statement = $command->getPdoStatement();
			$command->execute();
		}
		return $statement;
	}

	/**
	 * Get SQL order clause.
	 *
	 * @param array $order order by and order direction in form [[by1, direction1], [by2, direction2]...etc.]
	 * @param boolean $with_clause return with 'ORDER BY' clause
	 * @return string SQL order clause with values or empty string if no order criteria given
	 */
	public static function getOrder(array $order, $with_clause = true) {
		$odr_str = '';
		$odr = [];
		// Prepare order_by and order_direction values
		$db_params = Prado::getApplication()->getModule('api_config')->getConfig('db');
		for ($i = 0; $i < count($order); $i++) {
			list($order_by, $order_direction) = $order[$i];
			if ($db_params['type'] === self::PGSQL_TYPE) {
			    $order_by = strtolower($order_by);
			}
			$sorder = strtoupper($order_direction);
			$odr[] = sprintf('%s %s', $order_by, $sorder);
		}
		if (count($odr) > 0) {
			$ob = $with_clause ? ' ORDER BY ' : '';
			$odr_str = $ob . implode(',', $odr) . ' ';
		}
		return $odr_str;
	}
}
?>
