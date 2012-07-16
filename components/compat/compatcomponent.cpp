/******************************************************************************
 * Icinga 2                                                                   *
 * Copyright (C) 2012 Icinga Development Team (http://www.icinga.org/)        *
 *                                                                            *
 * This program is free software; you can redistribute it and/or              *
 * modify it under the terms of the GNU General Public License                *
 * as published by the Free Software Foundation; either version 2             *
 * of the License, or (at your option) any later version.                     *
 *                                                                            *
 * This program is distributed in the hope that it will be useful,            *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of             *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the              *
 * GNU General Public License for more details.                               *
 *                                                                            *
 * You should have received a copy of the GNU General Public License          *
 * along with this program; if not, write to the Free Software Foundation     *
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA.             *
 ******************************************************************************/

#include "i2-compat.h"

using namespace icinga;

/**
 * Hint: The reason why we're using "\n" rather than std::endl is because
 * std::endl also _flushes_ the output stream which severely degrades
 * performance (see http://gcc.gnu.org/onlinedocs/libstdc++/manual/bk01pt11ch25s02.html).
 */

/**
 * Returns the name of the component.
 *
 * @returns The name.
 */
string CompatComponent::GetName(void) const
{
	return "compat";
}

/**
 * Starts the component.
 */
void CompatComponent::Start(void)
{
	m_StatusTimer = boost::make_shared<Timer>();
	m_StatusTimer->SetInterval(15);
	m_StatusTimer->OnTimerExpired.connect(boost::bind(&CompatComponent::StatusTimerHandler, this));
	m_StatusTimer->Start();
	m_StatusTimer->Reschedule(0);

	CIB::RequireInformation(CIB_Configuration);
	CIB::RequireInformation(CIB_ProgramStatus);
	CIB::RequireInformation(CIB_ServiceStatus);
}

/**
 * Stops the component.
 */
void CompatComponent::Stop(void)
{
}

void CompatComponent::DumpHostStatus(ofstream& fp, Host host)
{
	int state;
	if (!host.IsReachable())
		state = 2; /* unreachable */
	else if (!host.IsUp())
		state = 1; /* down */
	else
		state = 0; /* up */

	fp << "hoststatus {" << "\n"
	   << "\t" << "host_name=" << host.GetName() << "\n"
	   << "\t" << "has_been_checked=1" << "\n"
	   << "\t" << "should_be_scheduled=1" << "\n"
	   << "\t" << "check_execution_time=0" << "\n"
	   << "\t" << "check_latency=0" << "\n"
	   << "\t" << "current_state=" << state << "\n"
	   << "\t" << "state_type=1" << "\n"
	   << "\t" << "last_check=" << time(NULL) << "\n"
	   << "\t" << "next_check=" << time(NULL) << "\n"
	   << "\t" << "current_attempt=1" << "\n"
	   << "\t" << "max_attempts=1" << "\n"
	   << "\t" << "active_checks_enabled=1" << "\n"
	   << "\t" << "passive_checks_enabled=1" << "\n"
	   << "\t" << "last_update=" << time(NULL) << "\n"
	   << "\t" << "}" << "\n"
	   << "\n";
}

void CompatComponent::DumpHostObject(ofstream& fp, Host host)
{
	fp << "define host {" << "\n"
	   << "\t" << "host_name" << "\t" << host.GetName() << "\n"
	   << "\t" << "alias" << "\t" << host.GetAlias() << "\n"
	   << "\t" << "check_interval" << "\t" << 1 << "\n"
	   << "\t" << "retry_interval" << "\t" << 1 << "\n"
	   << "\t" << "max_check_attempts" << "\t" << 1 << "\n"
	   << "\t" << "active_checks_enabled" << "\t" << 1 << "\n"
	   << "\t" << "passive_checks_enabled" << "\t" << 1 << "\n";

	set<string> parents = host.GetParents();

	if (!parents.empty()) {
		fp << "\t" << "parents" << "\t";
		DumpStringList(fp, parents);
		fp << "\n";
	}

	fp << "\t" << "}" << "\n"
	   << "\n";
}

void CompatComponent::DumpServiceStatus(ofstream& fp, Service service)
{
	string output;
	string perfdata;
	time_t schedule_start = -1, schedule_end = -1;
	time_t execution_start = -1, execution_end = -1;
	if (service.HasLastCheckResult()) {
		CheckResult cr = service.GetLastCheckResult();
		output = cr.GetOutput();
		schedule_start = cr.GetScheduleStart();
		schedule_end = cr.GetScheduleEnd();
		execution_start = cr.GetExecutionStart();
		execution_end = cr.GetExecutionEnd();
		perfdata = cr.GetPerformanceDataRaw();
	}

	time_t execution_time = (execution_end - execution_start);
	time_t latency = (schedule_end - schedule_start) - execution_time;

	int state = service.GetState();

	if (!service.IsReachable()) {
		state = StateCritical;
		
		string text = "One or more parent services are unavailable.";

		if (output.empty())
			output = text;
		else
			output = text + " (" + output + ")";
	}

	if (state > StateUnknown)
		state = StateUnknown;

	fp << "servicestatus {" << "\n"
	   << "\t" << "host_name=" << service.GetHost().GetName() << "\n"
	   << "\t" << "service_description=" << service.GetAlias() << "\n"
	   << "\t" << "check_interval=" << service.GetCheckInterval() / 60.0 << "\n"
	   << "\t" << "retry_interval=" << service.GetRetryInterval() / 60.0 << "\n"
	   << "\t" << "has_been_checked=" << (service.HasLastCheckResult() ? 1 : 0) << "\n"
	   << "\t" << "should_be_scheduled=1" << "\n"
	   << "\t" << "check_execution_time=" << execution_time << "\n"
	   << "\t" << "check_latency=" << latency << "\n"
	   << "\t" << "current_state=" << state << "\n"
	   << "\t" << "state_type=" << service.GetStateType() << "\n"
	   << "\t" << "plugin_output=" << output << "\n"
	   << "\t" << "performance_data=" << perfdata << "\n"
	   << "\t" << "last_check=" << schedule_end << "\n"
	   << "\t" << "next_check=" << service.GetNextCheck() << "\n"
	   << "\t" << "current_attempt=" << service.GetCurrentCheckAttempt() << "\n"
	   << "\t" << "max_attempts=" << service.GetMaxCheckAttempts() << "\n"
	   << "\t" << "last_state_change=" << service.GetLastStateChange() << "\n"
	   << "\t" << "last_hard_state_change=" << service.GetLastHardStateChange() << "\n"
	   << "\t" << "last_update=" << time(NULL) << "\n"
	   << "\t" << "active_checks_enabled=1" << "\n"
	   << "\t" << "passive_checks_enabled=1" << "\n"
	   << "\t" << "}" << "\n"
	   << "\n";
}

void CompatComponent::DumpServiceObject(ofstream& fp, Service service)
{
	fp << "define service {" << "\n"
	   << "\t" << "host_name" << "\t" << service.GetHost().GetName() << "\n"
	   << "\t" << "service_description" << "\t" << service.GetAlias() << "\n"
	   << "\t" << "check_command" << "\t" << "check_i2" << "\n"
	   << "\t" << "check_interval" << "\t" << service.GetCheckInterval() / 60.0 << "\n"
	   << "\t" << "retry_interval" << "\t" << service.GetRetryInterval() / 60.0 << "\n"
	   << "\t" << "max_check_attempts" << "\t" << 1 << "\n"
	   << "\t" << "active_checks_enabled" << "\t" << 1 << "\n"
	   << "\t" << "passive_checks_enabled" << "\t" << 1 << "\n"
	   << "\t" << "}" << "\n"
	   << "\n";
}

/**
 * Periodically writes the status.dat and objects.cache files.
 */
void CompatComponent::StatusTimerHandler(void)
{
	ofstream statusfp;
	statusfp.open("status.dat.tmp", ofstream::out | ofstream::trunc);

	statusfp << "# Icinga status file" << "\n"
		 << "# This file is auto-generated. Do not modify this file." << "\n"
		 << "\n";

	statusfp << "info {" << "\n"
		 << "\t" << "created=" << time(NULL) << "\n"
		 << "\t" << "version=2.0" << "\n"
		 << "\t" << "}" << "\n"
		 << "\n";

	statusfp << "programstatus {" << "\n"
		 << "\t" << "daemon_mode=1" << "\n"
		 << "\t" << "program_start=" << IcingaApplication::GetInstance()->GetStartTime() << "\n"
		 << "\t" << "active_service_checks_enabled=1" << "\n"
		 << "\t" << "passive_service_checks_enabled=1" << "\n"
		 << "\t" << "active_host_checks_enabled=0" << "\n"
		 << "\t" << "passive_host_checks_enabled=0" << "\n"
		 << "\t" << "check_service_freshness=0" << "\n"
		 << "\t" << "check_host_freshness=0" << "\n"
		 << "\t" << "enable_flap_detection=1" << "\n"
		 << "\t" << "enable_failure_prediction=0" << "\n"
		 << "\t" << "active_scheduled_service_check_stats=" << CIB::GetTaskStatistics(60) << "," << CIB::GetTaskStatistics(5 * 60) << "," << CIB::GetTaskStatistics(15 * 60) << "\n"
		 << "\t" << "}" << "\n"
		 << "\n";

	ofstream objectfp;
	objectfp.open("objects.cache.tmp", ofstream::out | ofstream::trunc);

	objectfp << "# Icinga object cache file" << "\n"
		 << "# This file is auto-generated. Do not modify this file." << "\n"
		 << "\n";

	map<string, vector<string> > hostgroups;

	ConfigObject::TMap::Range range;
	range = ConfigObject::GetObjects("host");

	ConfigObject::Ptr object;
	BOOST_FOREACH(tie(tuples::ignore, object), range) {
		Host host = object;

		Dictionary::Ptr dict;
		dict = host.GetGroups();

		if (dict) {
			Variant hostgroup;
			BOOST_FOREACH(tie(tuples::ignore, hostgroup), dict) {
				hostgroups[hostgroup].push_back(host.GetName());
			}
		}

		DumpHostStatus(statusfp, host);
		DumpHostObject(objectfp, host);
	}

	pair<string, vector<string > > hgt;
	BOOST_FOREACH(hgt, hostgroups) {
		const string& name = hgt.first;
		const vector<string>& hosts = hgt.second;

		objectfp << "define hostgroup {" << "\n"
			 << "\t" << "hostgroup_name" << "\t" << name << "\n";

		if (HostGroup::Exists(name)) {
			HostGroup hg = HostGroup::GetByName(name);
			objectfp << "\t" << "alias" << "\t" << hg.GetAlias() << "\n"
				 << "\t" << "notes_url" << "\t" << hg.GetNotesUrl() << "\n"
				 << "\t" << "action_url" << "\t" << hg.GetActionUrl() << "\n";
		}

		objectfp << "\t" << "members" << "\t";

		DumpStringList(objectfp, hosts);

		objectfp << "\n"
			 << "}" << "\n";
	}

	range = ConfigObject::GetObjects("service");

	map<string, vector<Service> > servicegroups;

	BOOST_FOREACH(tie(tuples::ignore, object), range) {
		Service service = object;

		Dictionary::Ptr dict;

		dict = service.GetGroups();

		if (dict) {
			Variant servicegroup;
			BOOST_FOREACH(tie(tuples::ignore, servicegroup), dict) {
				servicegroups[servicegroup].push_back(service);
			}
		}

		DumpServiceStatus(statusfp, service);
		DumpServiceObject(objectfp, service);
	}

	pair<string, vector<Service > > sgt;
	BOOST_FOREACH(sgt, servicegroups) {
		const string& name = sgt.first;
		const vector<Service>& services = sgt.second;

		objectfp << "define servicegroup {" << "\n"
			 << "\t" << "servicegroup_name" << "\t" << name << "\n";

		if (ServiceGroup::Exists(name)) {
			ServiceGroup sg = ServiceGroup::GetByName(name);
			objectfp << "\t" << "alias" << "\t" << sg.GetAlias() << "\n"
				 << "\t" << "notes_url" << "\t" << sg.GetNotesUrl() << "\n"
				 << "\t" << "action_url" << "\t" << sg.GetActionUrl() << "\n";
		}

		objectfp << "\t" << "members" << "\t";

		vector<string> sglist;
		vector<Service>::iterator vt;

		BOOST_FOREACH(const Service& service, services) {
			sglist.push_back(service.GetHost().GetName());
			sglist.push_back(service.GetAlias());
		}

		DumpStringList(objectfp, sglist);

		objectfp << "\n"
			 << "}" << "\n";
	}

	statusfp.close();
	rename("status.dat.tmp", "status.dat");

	objectfp.close();
	rename("objects.cache.tmp", "objects.cache");
}

EXPORT_COMPONENT(compat, CompatComponent);
