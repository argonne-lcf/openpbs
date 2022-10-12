# coding: utf-8

# Copyright (C) 1994-2021 Altair Engineering, Inc.
# For more information, contact Altair at www.altair.com.
#
# This file is part of both the OpenPBS software ("OpenPBS")
# and the PBS Professional ("PBS Pro") software.
#
# Open Source License Information:
#
# OpenPBS is free software. You can redistribute it and/or modify it under
# the terms of the GNU Affero General Public License as published by the
# Free Software Foundation, either version 3 of the License, or (at your
# option) any later version.
#
# OpenPBS is distributed in the hope that it will be useful, but WITHOUT
# ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
# FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Affero General Public
# License for more details.
#
# You should have received a copy of the GNU Affero General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.
#
# Commercial License Information:
#
# PBS Pro is commercially licensed software that shares a common core with
# the OpenPBS software.  For a copy of the commercial license terms and
# conditions, go to: (http://www.pbspro.com/agreement.html) or contact the
# Altair Legal Department.
#
# Altair's dual-license business model allows companies, individuals, and
# organizations to create proprietary derivative works of OpenPBS and
# distribute them - whether embedded or bundled with other software -
# under a commercial license agreement.
#
# Use of Altair's trademarks, including but not limited to "PBS™",
# "OpenPBS®", "PBS Professional®", and "PBS Pro™" and Altair's logos is
# subject to Altair's trademark licensing policies.


from tests.functional import *


class TestQdel(TestFunctional):
    """
    This test suite contains tests for qdel
    """

    def test_qdel_with_server_tagged_in_jobid(self):
        """
        Test to make sure that qdel uses server tagged in jobid instead of
        the PBS_SERVER conf setting
        """
        self.du.set_pbs_config(confs={'PBS_SERVER': 'not-a-server'})
        j = Job(TEST_USER)
        j.set_attributes({ATTR_q: 'workq@' + self.server.hostname})
        jid = self.server.submit(j)
        try:
            self.server.delete(jid)
        except PbsDeleteError as e:
            self.assertFalse(
                'Unknown Host' in e.msg[0],
                "Error message is not expected as server name is"
                "tagged in the jobid")
        self.du.set_pbs_config(confs={'PBS_SERVER': self.server.hostname})

    def test_qdel_unknown(self):
        """
        Test that qdel for an unknown job throws error saying the same
        """
        j = Job(TEST_USER)
        jid = self.server.submit(j)
        self.server.delete(jid, wait=True)
        try:
            self.server.delete(jid)
            self.fail("qdel didn't throw 'Unknown job id' error")
        except PbsDeleteError as e:
            self.assertEqual("qdel: Unknown Job Id " + jid, e.msg[0])

    # XXX: is this a valid test?
    #
    # def test_qdel_history_job(self):
    #     """
    #     Test deleting a history job after a custom resource is deleted
    #     The deletion of the history job happens in teardown
    #     """
    #     self.server.add_resource('foo')
    #     a = {'job_history_enable': 'True'}
    #     rc = self.server.manager(MGR_CMD_SET, SERVER, a)
    #     hook_body = "import pbs\n"
    #     hook_body += "e = pbs.event()\n"
    #     hook_body += "e.job.resources_used[\"foo\"] = \"10\"\n"
    #     a = {'event': 'execjob_epilogue', 'enabled': 'True'}
    #     self.server.create_import_hook("epi", a, hook_body)
    #     j = Job(TEST_USER)
    #     j.set_sleep_time(10)
    #     jid = self.server.submit(j)
    #     self.server.expect(JOB, {'job_state': 'R'}, id=jid)
    #     self.server.expect(JOB, {'job_state': 'F'}, id=jid,
    #                        extend='x', max_attempts=20)
    #     msg = "Resource allowed to be deleted"
    #     with self.assertRaises(PbsManagerError, msg=msg) as e:
    #         self.server.manager(MGR_CMD_DELETE, RSC, id="foo")
    #     m = "Resource busy on job"
    #     self.assertIn(m, e.exception.msg[0])
    #     self.server.delete(jid, extend='deletehist')

    def test_qdel_arrayjob_in_transit(self):
        """
        Test the array job deletion
        soon after they have been signalled for running.
        """
        self.server.manager(MGR_CMD_SET, SERVER, {'scheduling': 'false'})
        a = {'resources_available.ncpus': 6}
        self.server.manager(MGR_CMD_SET, NODE, a, self.mom.shortname)
        j = Job(TEST_USER, attrs={
            ATTR_J: '1-3', 'Resource_List.select': 'ncpus=1'})
        job_set = []
        for i in range(4):
            job_set.append(self.server.submit(j))
        self.server.manager(MGR_CMD_SET, SERVER, {'scheduling': 'true'})
        self.server.delete(job_set)
        # Make sure that the counters are not going negative
        msg = "job*has already been deleted from delete job list"
        self.scheduler.log_match(msg, existence=False,
                                 max_attempts=3, regexp=True)
        # Make sure the last two jobs doesn't started running
        # while the deletion is in process
        for job in job_set[2:]:
            jobid, server = job.split('.')
            arrjob = jobid[-2:] + '[1]' + server
            msg = arrjob + ";Job Run at request of Scheduler"
            self.scheduler.log_match(msg, existence=False, max_attempts=3)

    def test_qdel_history_job_rerun(self):
        """
        Test rerunning a history job that was prematurely terminated due
        to a a downed mom.
        """
        a = {'job_history_enable': 'True', 'job_history_duration': '5',
             'job_requeue_timeout': '5', 'node_fail_requeue': '5',
             'scheduler_iteration': '5'}
        self.server.manager(MGR_CMD_SET, SERVER, a)
        j = Job()
        j.set_sleep_time(30)
        jid = self.server.submit(j)
        self.server.expect(JOB, {'job_state': 'R'}, id=jid)
        self.server.manager(MGR_CMD_SET, SERVER, {'scheduling': 'False'})
        self.mom.stop()

        # Force job to be prematurely terminated
        try:
            self.server.deljob(jid)
        except PbsDeljobError as e:
            err_msg = "could not connect to MOM"
            self.assertTrue(err_msg in e.msg[0],
                            "Did not get the expected message")
            self.assertTrue(e.rc != 0, "Exit code shows success")
        else:
            raise self.failureException("qdel job did not return error")

        self.server.expect(JOB, {'job_state': 'Q'}, id=jid)
        self.mom.start()
        self.server.manager(MGR_CMD_SET, SERVER, {'scheduling': 'True'})

        # Upon rerun, finished status should be '92' (Finished)
        a = {'job_state': 'F', 'substate': '92'}
        self.server.expect(JOB, a, extend='x',
                           offset=1, id=jid, interval=1)

    def test_qdel_history_job_rerun_nx(self):
        """
        Test rerunning a history job that was prematurely terminated due
        to a a downed mom.
        """
        a = {'job_history_enable': 'True', 'job_history_duration': '5',
             'job_requeue_timeout': '5', 'node_fail_requeue': '5',
             'scheduler_iteration': '5'}
        self.server.manager(MGR_CMD_SET, SERVER, a)
        j = Job()
        j.set_sleep_time(30)
        jid = self.server.submit(j)
        self.server.expect(JOB, {'job_state': 'R'}, id=jid)
        self.server.manager(MGR_CMD_SET, SERVER, {'scheduling': 'False'})
        self.mom.stop()

        # Force job to be prematurely terminated and try to delete it more than
        # once.
        try:
            self.server.deljob([jid, jid, jid, jid])
        except PbsDeljobError as e:
            err_msg = "could not connect to MOM"
            self.assertTrue(err_msg in e.msg[0],
                            "Did not get the expected message")
            self.assertTrue(e.rc != 0, "Exit code shows success")
        else:
            raise self.failureException("qdel job did not return error")

        self.server.expect(JOB, {'job_state': 'Q'}, id=jid)
        self.mom.start()
        self.server.manager(MGR_CMD_SET, SERVER, {'scheduling': 'True'})

        # Upon rerun, finished status should be '92' (Finished)
        a = {'job_state': 'F', 'substate': '92'}
        self.server.expect(JOB, a, extend='x',
                           offset=1, id=jid, interval=1)

    # TODO: add rerun nx for multiple job arrays

    def test_qdel_same_jobid_nx_00(self):
        """
        Test that qdel that deletes the job more than once in the same line.
        """
        a = {'job_history_enable': 'True'}
        rc = self.server.manager(MGR_CMD_SET, SERVER, a)
        j = Job(TEST_USER)
        jid = self.server.submit(j)
        self.server.expect(JOB, {ATTR_state: "R"}, id=jid)
        self.server.delete([jid, jid, jid, jid, jid], wait=True)
        self.server.expect(JOB, {'job_state': 'F', 'substate': 91}, id=jid,
                           extend='x', max_attempts=20)

    def test_qdel_same_jobid_nx_01(self):
        """
        Test that qdel that deletes the job more than once in the same line.
        Done twice.
        """
        a = {'job_history_enable': 'True'}
        rc = self.server.manager(MGR_CMD_SET, SERVER, a)
        j = Job(TEST_USER)
        jid = self.server.submit(j)
        self.server.expect(JOB, {ATTR_state: "R"}, id=jid)
        self.server.delete([jid, jid], wait=True)
        self.server.expect(JOB, {'job_state': 'F', 'substate': 91}, id=jid,
                           extend='x', max_attempts=20)

        # this may take 2 or more times to break.
        j = Job(TEST_USER)
        jid = self.server.submit(j)
        self.server.expect(JOB, {ATTR_state: "R"}, id=jid)
        self.server.delete([jid, jid], wait=True)
        self.server.expect(JOB, {'job_state': 'F', 'substate': 91}, id=jid,
                           extend='x', max_attempts=20)

        j = Job(TEST_USER)
        jid = self.server.submit(j)
        self.server.expect(JOB, {ATTR_state: "R"}, id=jid)
        self.server.delete([jid, jid], wait=True)
        self.server.expect(JOB, {'job_state': 'F', 'substate': 91}, id=jid,
                           extend='x', max_attempts=20)

        j = Job(TEST_USER)
        jid = self.server.submit(j)
        self.server.expect(JOB, {ATTR_state: "R"}, id=jid)
        self.server.delete([jid, jid], wait=True)
        self.server.expect(JOB, {'job_state': 'F', 'substate': 91}, id=jid,
                           extend='x', max_attempts=20)

    def test_qdel_same_jobid_nx_02(self):
        """
        Test that qdel that deletes the job more than once in the same line.
        With rerun.
        """
        a = {'job_history_enable': 'True'}
        rc = self.server.manager(MGR_CMD_SET, SERVER, a)
        attrs = {ATTR_r: 'y'}
        j = Job(TEST_USER, attrs=attrs)
        jid = self.server.submit(j)
        self.server.expect(JOB, {ATTR_state: "R"}, id=jid)
        self.server.delete([jid, jid, jid, jid, jid], wait=True)
        self.server.expect(JOB, {'job_state': 'F', 'substate': 91}, id=jid,
                           extend='x', max_attempts=20)

    def test_qdel_same_jobid_nx_array_00(self):
        """
        Test that qdel that deletes the array job more than once in the same
        line.
        """
        a = {'job_history_enable': 'True'}
        rc = self.server.manager(MGR_CMD_SET, SERVER, a)
        j = Job(TEST_USER, attrs={ATTR_J: '1-2'})
        jid = self.server.submit(j)
        sjid1 = j.create_subjob_id(jid, 1)
        self.server.expect(JOB, {ATTR_state: "R"}, id=sjid1)
        self.server.delete([jid, jid, jid, jid, jid], wait=True)
        self.server.expect(JOB, {'job_state': 'F', 'substate': 91}, id=jid,
                           extend='x', max_attempts=20)

    def test_qdel_same_jobid_nx_array_01(self):
        """
        Test that qdel that deletes the array job more than once in the same
        line.
        """
        a = {'job_history_enable': 'True'}
        rc = self.server.manager(MGR_CMD_SET, SERVER, a)
        j = Job(TEST_USER, attrs={ATTR_J: '0-734:512'})
        jid = self.server.submit(j)
        sjid1 = j.create_subjob_id(jid, 0)
        self.server.expect(JOB, {ATTR_state: "R"}, id=sjid1)
        self.server.delete([jid, jid, jid, jid, jid], wait=True)
        self.server.expect(JOB, {'job_state': 'F'}, id=jid,
                           extend='x', max_attempts=60)

    def test_qdel_same_jobid_nx_array_subjob_00(self):
        """
        Test that qdel that deletes and array subjob more than once in the same
        line.
        """
        a = {'job_history_enable': 'True'}
        rc = self.server.manager(MGR_CMD_SET, SERVER, a)
        j = Job(TEST_USER, attrs={ATTR_J: '1-2'})
        j.set_sleep_time(20)
        jid = self.server.submit(j)

        sjid1 = j.create_subjob_id(jid, 1)
        sjid2 = j.create_subjob_id(jid, 2)
        self.server.expect(JOB, {ATTR_state: "R"}, id=sjid1)
        self.server.expect(JOB, {ATTR_state: "R"}, id=sjid2)

        self.server.delete([sjid1, sjid1, sjid1, sjid1, sjid1], wait=True)
        self.server.expect(JOB, {'job_state': 'F'}, id=jid,
                           extend='x', max_attempts=60)

    def test_qdel_same_jobid_nx_array_subjob_01(self):
        """
        Test that qdel that deletes and array subjob more than once in the same
        line.
        """
        a = {'job_history_enable': 'True'}
        rc = self.server.manager(MGR_CMD_SET, SERVER, a)
        j = Job(TEST_USER, attrs={ATTR_J: '0-734:512'})
        j.set_sleep_time(20)
        jid = self.server.submit(j)

        sjid1 = j.create_subjob_id(jid, 0)
        sjid2 = j.create_subjob_id(jid, 512)
        self.server.expect(JOB, {ATTR_state: "R"}, id=sjid1)
        self.server.expect(JOB, {ATTR_state: "R"}, id=sjid2)

        self.server.delete([sjid1, sjid1, sjid1, sjid1, sjid1], wait=True)
        self.server.expect(JOB, {'job_state': 'F'}, id=jid,
                           extend='x', max_attempts=60)

    def test_qdel_same_jobid_nx_array_subjob_02(self):
        """
        Test that qdel that deletes and array subjob more than once in the same
        line but uses ranges.
        """
        a = {
            'log_events': 4095,
            'job_history_enable': 'True'
        }
        self.server.manager(MGR_CMD_SET, SERVER, a)
        a = {'resources_available.ncpus': 16}
        for mom in self.moms.values():
            self.server.manager(MGR_CMD_SET, NODE, a, mom.shortname)
        j = Job(TEST_USER, attrs={ATTR_J: '1-7'})
        j.set_sleep_time(20)
        jid = self.server.submit(j)

        sjid1 = j.create_subjob_id(jid, 1)
        sjid2 = j.create_subjob_id(jid, 2)
        sjid3 = j.create_subjob_id(jid, 3)
        sjid4 = j.create_subjob_id(jid, 4)
        sjid5 = j.create_subjob_id(jid, 5)
        sjid6 = j.create_subjob_id(jid, 6)
        sjid7 = j.create_subjob_id(jid, 7)
        self.server.expect(JOB, {ATTR_state: "R"}, id=sjid1)
        self.server.expect(JOB, {ATTR_state: "R"}, id=sjid2)
        self.server.expect(JOB, {ATTR_state: "R"}, id=sjid3)
        self.server.expect(JOB, {ATTR_state: "R"}, id=sjid4)
        self.server.expect(JOB, {ATTR_state: "R"}, id=sjid5)
        self.server.expect(JOB, {ATTR_state: "R"}, id=sjid6)
        self.server.expect(JOB, {ATTR_state: "R"}, id=sjid7)

        jid_array = jid.split("[")[0]

        self.server.delete([f"{jid_array}[2-4]", f"{jid_array}[3-5]"] * 10, wait=True)
        self.server.expect(JOB, {'job_state': 'F'}, id=jid,
                           extend='x', max_attempts=60)

    def test_qdel_same_jobid_nx_array_subjob_03(self):
        """
        Test that qdel that deletes and array subjob more than once in the same
        line but uses ranges.
        """
        a = {
            'log_events': 4095,
            'job_history_enable': 'True'
        }
        self.server.manager(MGR_CMD_SET, SERVER, a)
        a = {'resources_available.ncpus': 16}
        for mom in self.moms.values():
            self.server.manager(MGR_CMD_SET, NODE, a, mom.shortname)
        j = Job(TEST_USER, attrs={ATTR_J: '1-7'})
        j.set_sleep_time(20)
        jid = self.server.submit(j)

        sjid1 = j.create_subjob_id(jid, 1)
        sjid2 = j.create_subjob_id(jid, 2)
        sjid3 = j.create_subjob_id(jid, 3)
        sjid4 = j.create_subjob_id(jid, 4)
        sjid5 = j.create_subjob_id(jid, 5)
        sjid6 = j.create_subjob_id(jid, 6)
        sjid7 = j.create_subjob_id(jid, 7)
        self.server.expect(JOB, {ATTR_state: "R"}, id=sjid1)
        self.server.expect(JOB, {ATTR_state: "R"}, id=sjid2)
        self.server.expect(JOB, {ATTR_state: "R"}, id=sjid3)
        self.server.expect(JOB, {ATTR_state: "R"}, id=sjid4)
        self.server.expect(JOB, {ATTR_state: "R"}, id=sjid5)
        self.server.expect(JOB, {ATTR_state: "R"}, id=sjid6)
        self.server.expect(JOB, {ATTR_state: "R"}, id=sjid7)

        jid_array = jid.split("[")[0]

        self.server.delete([f"{jid_array}[2-4]", f"{jid_array}[3-5]"] * 10, wait=False)
        self.server.delete([f"{jid_array}[2-4]", f"{jid_array}[3-5]"] * 10, wait=False)
        self.server.delete([f"{jid_array}[2-4]", f"{jid_array}[3-5]"] * 10, wait=True)
        self.server.expect(JOB, {'job_state': 'F'}, id=jid,
                           extend='x', max_attempts=60)

    def test_qdel_same_jobid_nx_array_subjob_04(self):
        """
        Test that qdel that deletes and array subjob more than once in the same
        line but uses ranges.
        """
        a = {
            'log_events': 4095,
            'job_history_enable': 'True'
        }
        self.server.manager(MGR_CMD_SET, SERVER, a)
        a = {'resources_available.ncpus': 6}
        for mom in self.moms.values():
            self.server.manager(MGR_CMD_SET, NODE, a, mom.shortname)
        j = Job(TEST_USER, attrs={ATTR_J: '1-10'})
        j.set_sleep_time(20)
        jid = self.server.submit(j)

        sjid1 = j.create_subjob_id(jid, 1)
        sjid2 = j.create_subjob_id(jid, 2)
        sjid3 = j.create_subjob_id(jid, 3)
        sjid4 = j.create_subjob_id(jid, 4)
        sjid5 = j.create_subjob_id(jid, 5)
        sjid6 = j.create_subjob_id(jid, 6)
        sjid7 = j.create_subjob_id(jid, 7)
        self.server.expect(JOB, {ATTR_state: "R"}, id=sjid1)
        self.server.expect(JOB, {ATTR_state: "R"}, id=sjid2)
        self.server.expect(JOB, {ATTR_state: "R"}, id=sjid3)
        self.server.expect(JOB, {ATTR_state: "R"}, id=sjid4)
        self.server.expect(JOB, {ATTR_state: "R"}, id=sjid5)
        self.server.expect(JOB, {ATTR_state: "R"}, id=sjid6)
        self.server.expect(JOB, {ATTR_state: "R"}, id=sjid7)

        jid_array = jid.split("[")[0]

        self.server.delete([f"{jid_array}[2-4]", f"{jid_array}[3-5]"] * 10, wait=True)
        self.server.expect(JOB, {'job_state': 'F'}, id=jid,
                           extend='x', max_attempts=60)
