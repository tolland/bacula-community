<?php
/*
 * Bacula(R) - The Network Backup Solution
 * Baculum   - Bacula web interface
 *
 * Copyright (C) 2013-2022 Kern Sibbald
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
use Prado\Prado;
use Baculum\API\Modules\ConsoleOutputPage;
Prado::using('Baculum.API.Pages.API.JobsShow');

/**
 * Source (client + fileset + job) manager module.
 *
 * @author Marcin Haba <marcin.haba@bacula.pl>
 * @category Module
 * @package Baculum API
 */
class SourceManager extends APIModule {

	/**
	 * Source result modes.
	 * Modes:
	 *  - normal - record list without any additional data
	 *  - overview - record list in sections (vSphere, MySQL, PostgreSQL...) with items and summary count
	 */
	const SOURCE_RESULT_MODE_NORMAL = 'normal';
	const SOURCE_RESULT_MODE_OVERVIEW = 'overview';

	public function getSources($criteria = [], $props = [], $limit_val = 0, $offset_val = 0, $order_by = null, $order_direction = 'DESC', $mode = self::SOURCE_RESULT_MODE_NORMAL) {
		$jobs_show = new \JobsShow;
		$config = $jobs_show->show(
			ConsoleOutputPage::OUTPUT_FORMAT_JSON
		);

		$fs_content = $this->getFileSetAndContent();
		$fs_count = [];

		$sources = $jobs = [];
		if ($config->exitcode === 0) {
			for ($i = 0; $i < count($config->output); $i++) {
				if (!key_exists('name', $config->output[$i])) {
					continue;
				}
				if ($config->output[$i]['jobtype'] != 66) {
					// only backup jobs are taken into account in sources
					continue;
				}
				$job = $config->output[$i]['name'];
				$fileset = $config->output[$i]['fileset'];
				$client = $config->output[$i]['client'];

				// Use filters
				if (key_exists('job', $props) && $props['job'] != $job) {
					continue;
				} elseif (key_exists('fileset', $props) && $props['fileset'] != $fileset) {
					continue;
				} elseif (key_exists('client', $props) && $props['client'] != $client) {
					continue;
				}

				$jobs[] = $job;

				$sources[] = [
					'job' => $job,
					'client' => $client,
					'fileset' => $fileset
				];
			}
		}

		$where_job = count($jobs) > 0 ? ' Job.Name IN (\'' . implode('\',\'', $jobs) . '\') ' : '';

		$where = Database::getWhere($criteria, true);
		$sql = 'SELECT 
	ores.job       AS job,
	jres.StartTime AS starttime,
	jres.EndTime   AS endtime,
	ores.jobid     AS jobid,
	regexp_split_to_table(fres.Content, \',\') AS content,
	jres.Type      AS type,
	jres.Level     AS level,
	jres.JobStatus AS jobstatus,
	jres.JobErrors AS joberrors
	FROM Job AS jres,
	     FileSet AS fres,
 	(
		SELECT
			MAX(JobId) AS jobid,
			FileSet.FileSet AS fileset,
			Client.Name AS client,
			Job.Name AS job
		FROM Job
		JOIN FileSet USING (FileSetId)
		JOIN Client USING (ClientId)
		' . (!empty($where_job) ? ' WHERE ' . $where_job : '') . ' 
		GROUP BY FileSet.FileSet, Client.Name, Job.Name
	) AS ores
	LEFT JOIN Object USING (JobId)
	WHERE
		jres.JobId = ores.jobid
		AND jres.FileSetId = fres.FileSetId
		AND jres.Type = \'B\'
		' . (!empty($where['where']) ? ' AND ' .  $where['where']  : '') . '
	ORDER BY jres.starttime ASC';

		$statement = Database::runQuery($sql, $where['params']);
		$result = $statement->fetchAll(PDO::FETCH_GROUP | PDO::FETCH_ASSOC);

		/*
		 * This query is only to know if source is filtered.
		 * Otherwise there is not possible to make a difference between
		 * sources filtered and sources that have not been exected yet.
		 * It there will be an other way to make this difference, please
		 * remove this query.
		 */
		$sql = 'SELECT DISTINCT
			Job.Name AS job,
			FileSet.FileSet AS fileset,
			Client.Name AS client
		FROM Job
		JOIN FileSet USING (FileSetId)
		JOIN Client USING (ClientId)
		WHERE Job.Type = \'B\'';
		$statement = Database::runQuery($sql);
		$jobs_all = $statement->fetchAll(PDO::FETCH_GROUP | PDO::FETCH_ASSOC);

		function is_item($data, $item, $type) {
			$found = false;
			for ($i = 0; $i < count($data); $i++) {
				if ($data[$i][$type] === $item) {
					$found = true;
					break;
				}
			}
			return $found;
		}

		$sources_len = count($sources);
		$sources_ft = [];
		for ($i = 0; $i < $sources_len; $i++) {
			if (key_exists($sources[$i]['job'], $result)) {
				// job has jobids in the catalog - job was executed
				$sources[$i] = array_merge($sources[$i], $result[$sources[$i]['job']][0]);

			} elseif (key_exists($sources[$i]['job'], $jobs_all) && is_item($jobs_all[$sources[$i]['job']], $sources[$i]['client'], 'client') && is_item($jobs_all[$sources[$i]['job']], $sources[$i]['fileset'], 'fileset')) {
				// Source exists but it has been filtered
				continue;
			} elseif (count($criteria) > 0) {
				 // If SQL criteria used, all sources that has not been executed anytime are hidden.
				continue;
			} else {
				// No jobs in the catalog - source was not executed and no SQL criteria used.
				$sources[$i]['starttime'] = '';
				$sources[$i]['endtime'] = '';
				$sources[$i]['jobid'] = 0;
				$sources[$i]['content'] = '';
				$sources[$i]['type'] = '';
				$sources[$i]['level'] = '';
				$sources[$i]['jobstatus'] = '';
				$sources[$i]['joberrors'] = '';
			}

			// Set count for each section. It has to be done for all sources here.
			$fileset = $sources[$i]['fileset'];
			if (key_exists($fileset, $fs_content)) {
				for ($j = 0; $j < count($fs_content[$fileset]); $j++) {
					$content = $fs_content[$fileset][$j]['content'];
					if (!key_exists($content, $fs_count)) {
						$fs_count[$content] = 0;
					}
					$fs_count[$content]++;
				}
				$cb = function($el) {
					return $el['content'];
				};
				$cnts = array_map($cb, $fs_content[$fileset]);
				$sources[$i]['content'] = implode(',', $cnts);
			}
			$sources_ft[] = $sources[$i];
		}

		// below work only with filtered jobs
		$sources = $sources_ft;

		$misc = $this->getModule('misc');

		// Sort items if needed
		if (is_string($order_by)) {
			// Sort all items
			if ($order_by == 'jobstatus') {
				// for jobstatus sort also by joberrors
				$misc::sortResultsByField(
					$sources,
					$order_by,
					$order_direction,
					'joberrors',
					$order_direction
				);
			} else {
				$misc::sortResultsByField(
					$sources,
					$order_by,
					$order_direction
				);
			}
		}

		if ($mode == self::SOURCE_RESULT_MODE_OVERVIEW) {
			// Overview mode.
			$res = [];
			$ofs = [];
			$sources_len = count($sources);
			for ($i = 0; $i < $sources_len; $i++) {
				$content = $fs_content[$sources[$i]['fileset']];
				$content_len = count($content);
				for ($j = 0; $j < $content_len; $j++) {
					if (!key_exists($content[$j]['content'], $res)) {
						$res[$content[$j]['content']] = [
							'sources' => [],
							'count' => $fs_count[$content[$j]['content']]
						];
						$ofs[$content[$j]['content']] = 0;
					}
					if ($offset_val > 0 && $ofs[$content[$j]['content']] < $offset_val) {
						// offset taken into account
						$ofs[$content[$j]['content']]++;
						continue;
					}
					if ($limit_val > 0 && count($res[$content[$j]['content']]['sources']) >= $limit_val) {
						// limit reached
						break;
					}
					$res[$content[$j]['content']]['sources'][] = $sources[$i];
				}
			}
			$sources = $res;
		} else {
			if ($limit_val > 0) {
				$sources = array_slice($sources, $offset_val, $limit_val);
			} elseif ($limit_val == 0 && $offset_val > 0) {
				$sources = array_slice($sources, $offset_val);
			}
		}
		return $sources;
	}

	/**
	 * Get fileset and content.
	 *
	 * @return array content counts
	 */
	public function getFileSetAndContent() {
		$sql = 'SELECT DISTINCT FileSet.FileSet AS fileset,
				regexp_split_to_table(FileSet.Content, \',\') AS content
			FROM
				FileSet';
		$statement = Database::runQuery($sql);
		return $statement->fetchAll(PDO::FETCH_GROUP | PDO::FETCH_ASSOC);
	}
}
?>
