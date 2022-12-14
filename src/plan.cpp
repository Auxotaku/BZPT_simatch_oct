#include "nubot/nubot_control/plan.h"
#include "core.hpp"
using namespace nubot;

Plan::Plan() : our_goal(DPoint(-1100.0, 0.0)), opp_goal(DPoint(1100.0, 0.0))
{
	shoot_out = false;
	isinposition_ = false;
	action = nullptr;
	for (int i = 0; i < 4; ++i)
	{
		our_near_ball_sort_ID[i] = opp_near_ball_sort_ID[i] = i + 2;
	}
}

bool Plan::penalty()
{
	bool shoot_flag = false;
	action->move_action = TurnForShoot;
	action->rotate_acton = TurnForShoot;
	action->rotate_mode = 0;

	DPoint tmp = DPoint(1100.0, 0.0);
	if (m_behaviour_.move2oriFAST((tmp - robot_pos_).angle().radian_, robot_ori_.radian_))
	{
		action->shootPos = -1;
		action->strength = sqrt((((tmp - robot_pos_).length() / 100.0) + 1.2) * 9.8);
		shoot_flag = true;
		std::cout << "shoot done " << std::endl;
	}

	return shoot_flag;
}

void Plan::update_a2b_radian()
{
	for (int i = 0; i < 5; i++)
	{
		past_radian[i].push(ball_pos_ - world_model_->Opponents_[i]);
		if (past_radian[i].size() > 6)
			past_radian[i].pop();
	}
}

void Plan::update_opp_location()
{
	for (int i = 0; i < 5; i++)
	{
		opp_location[i].push(world_model_->Opponents_[i]);
		if (opp_location[i].size() > 6)
			opp_location[i].pop();
	}
}

void Plan::update_past_velocity()
{
	past_velocity.push(ball_vel_);
	if (past_velocity.size() > 10)
		past_velocity.pop();
}

DPoint Plan::get_my_vel()
{
	my_location.push(robot_pos_);
	if (my_location.size() > 6)
		my_location.pop();
	DPoint my_vel(0, 0);
	if (my_location.size() != 6)
		return my_vel;
	double v_x, v_y;
	std::queue<DPoint> q = my_location;
	int t = 0;
	DPoint my_pos[6], s;
	while (!q.empty())
	{
		s = q.front();
		my_pos[t++] = s;
		q.pop();
	}
	for (int k = 0; k < 3; k++)
	{
		v_x += (my_pos[k + 3].x_ - my_pos[k].x_) / 3;
		v_y += (my_pos[k + 3].y_ - my_pos[k].y_) / 3;
	}
	my_vel.x_ = v_x / 0.09;
	my_vel.y_ = v_y / 0.09;
	return my_vel;
}

DPoint Plan::get_opp_vel(int i)
{
	DPoint opp_vel(0, 0);
	if (opp_location[i].size() != 6)
		return opp_vel;
	double v_x, v_y;
	std::queue<DPoint> q = opp_location[i];
	int t = 0;
	DPoint opp_pos[6], s;
	while (!q.empty())
	{
		s = q.front();
		opp_pos[t++] = s;
		q.pop();
	}
	for (int k = 0; k < 3; k++)
	{
		v_x += (opp_pos[k + 3].x_ - opp_pos[k].x_) / 3;
		v_y += (opp_pos[k + 3].y_ - opp_pos[k].y_) / 3;
	}
	opp_vel.x_ = v_x / 0.09;
	opp_vel.y_ = v_y / 0.09;
	return opp_vel;
}

double Plan::get_opp_ori(int i)
{
	if (past_radian[i].size() != 10)
		return 0;
	std::queue<DPoint> a2b = past_radian[i];
	int t = 0;
	double radian[10], delta_radian;
	while (!a2b.empty())
	{
		radian[t++] = a2b.front().angle().radian_;
		a2b.pop();
	}
	for (int k = 0; k < 5; k++)
	{
		delta_radian += (radian[k + 5] - radian[k]) / 5;
	}
	delta_radian /= 5;
	return delta_radian / 0.03;
}

void Plan::predict_attackerwithball(int i)
{
	DPoint attacker_pos_now = world_model_->Opponents_[i];
	DPoint attacker_vel = get_opp_vel(i);
	double a2b_radian = (ball_pos_ - attacker_pos_now).angle().radian_;
	double a2b_ori = get_opp_ori(i);
	double a2b_radian_next;
	attacker_next_pos = attacker_pos_now + 0.3 * attacker_vel;
	a2b_radian_next = a2b_radian + 0.3 * a2b_ori;
	a2b_next = DPoint(cos(a2b_radian_next), sin(a2b_radian_next));
}

// catchball , useless
void Plan::catchBallSlowly()
{
	auto r2b = ball_pos_ - robot_pos_;
	if (m_behaviour_.move2orif(r2b.angle().radian_, robot_ori_.radian_))
	{
		m_behaviour_.move2target_slow(ball_pos_, robot_pos_);
		action->handle_enable = 1;
		action->move_action = CatchBall_slow;
		action->rotate_acton = CatchBall_slow;
		action->rotate_mode = 0;
	}
}

void Plan::init_dis()
{
	for (int i = 0; i < 5; ++i)
		for (int j = 0; j < 5; ++j)
			dis[i][j] = world_model_->Opponents_[i].distance(world_model_->RobotInfo_[j].getLocation());
}

void Plan::sortourID()
{
	for (int i = 2; i >= 0; i--)
	{
		for (int j = 0; j <= i; j++)
		{
			double jlen = (world_model_->RobotInfo_[(our_near_ball_sort_ID[j]) - 1].getLocation() - ball_pos_).length();
			double jpluslen = (world_model_->RobotInfo_[(our_near_ball_sort_ID[j + 1]) - 1].getLocation() - ball_pos_).length();
			if (ifParking(world_model_->RobotInfo_[(our_near_ball_sort_ID[j]) - 1].getLocation()))
			{
				jlen += 4000.0;
			}
			if (ifParking(world_model_->RobotInfo_[(our_near_ball_sort_ID[j + 1]) - 1].getLocation()))
			{
				jpluslen += 4000.0;
			}
			if (jlen > jpluslen)
			{
				std::swap(our_near_ball_sort_ID[j + 1], our_near_ball_sort_ID[j]);
			}
		}
	}
}

void Plan::sortoppID()
{
	for (int i = 2; i >= 0; i--)
	{ // sort the length of opp robots' distances to ball, the [0] is the nearest
		for (int j = 0; j <= i; j++)
		{
			double jlen = (world_model_->Opponents_[(opp_near_ball_sort_ID[j]) - 1] - ball_pos_).length();
			double jpluslen = (world_model_->Opponents_[(opp_near_ball_sort_ID[j + 1]) - 1] - ball_pos_).length();
			if (ifParking(world_model_->Opponents_[(opp_near_ball_sort_ID[j]) - 1]))
			{
				jlen += 4000.0;
			}
			if (ifParking(world_model_->Opponents_[(opp_near_ball_sort_ID[j + 1]) - 1]))
			{
				jpluslen += 4000.0;
			}
			if (jlen > jpluslen)
			{
				std::swap(opp_near_ball_sort_ID[j + 1], opp_near_ball_sort_ID[j]);
			}
		}
	}
}

int Plan::Get_Ball_Mode(int state)
{
	static int loop_num_ = 6;
	static double start_vel_ = ball_vel_.length(); // record the vel last

	if (state == 1) // start mode
	{
		// init it
		start_vel_ = ball_vel_.length();
		loop_num_ = 6;
	}
	else // test mode
	{
		if (loop_num_ == 0)
		{
			int shoot_mode;
			if (fabs(ball_vel_.length() - start_vel_) < 1.0)
			{
				shoot_mode = FLY;
			}
			else
			{
				shoot_mode = RUN;
			}
			return shoot_mode;
		}
		else
		{
			loop_num_--;
			return 0; // is waiting and test
		}
	}
}

int Plan::ourDribble()
{
	for (int i = 0; i < 5; ++i)
		if (world_model_->RobotInfo_[i].getDribbleState())
			return i;
	return -1;
}

const DPoint Plan::start_point()
{
	static DPoint begin_point = DPoint(10000.0, 10000.0);
	if (ourDribble() > -1 || oppDribble() > -1)
	{
		begin_point = ball_pos_;
	}
	return begin_point;
}

bool Plan::ball_is_flying()
{
	if (past_velocity.size() != 10)
		return false;
	std::queue<DPoint> q = past_velocity;
	DPoint v;
	double past_velocity_[20], average_delta_v, maxvel = 0.0;
	int t = 0;
	while (!q.empty())
	{
		v = q.front();
		past_velocity_[++t] = sqrt(v.x_ * v.x_ + v.y_ * v.y_);
		q.pop();
	}
	for (int i = 1; i <= 10; i++)
	{
		if (past_velocity_[i] >= maxvel)
			maxvel = past_velocity_[i];
	}
	if (maxvel <= 0.1)
		return false;
	for (int i = 1; i <= 5; i++)
	{
		average_delta_v += (past_velocity_[i + 5] - past_velocity_[i]) / 5;
	}
	average_delta_v /= 5;
	return std::abs(average_delta_v) <= 0.01;
}

const DPoint Plan::landing_pos()
{
	DPoint flying_ball_pos;
	DPoint begin_point = start_point();
	flying_ball_pos = DPoint(-10000.0, -10000.0);
	if (ball_is_flying())
	{
		flying_ball_pos = begin_point + (ball_vel_.length() / 26.0) * (ball_vel_.length() / 26.0) / (ball_vel_.length()) * ball_vel_;
	}
	return flying_ball_pos;
}

const DPoint Plan::get_fly_landing_point()
{
	static double vel_change = ball_vel_.length();
	static int mode = 0; // 0 is waiting mode, 1 is record mode
	static DPoint begin(0.0, 0.0);
	static DPoint target(0.0, 0.0);

	if (mode == 0 && (ball_vel_.length() - vel_change) > 100.0) // begin to kick ball
	{
		begin = ball_pos_;
		mode = 1;
		Get_Ball_Mode(1);
		target = DPoint(-10000.0, -10000.0);
	}
	else if (mode == 1)
	{
		int ball_mode = Get_Ball_Mode(0);
		int Flg = 1; // 1 present can be determined as shoot, 0 present can not

		for (int i = 0; i < 5; i++)
		{
			if ((world_model_->RobotInfo_[i].getLocation() - ball_pos_).length() < 40.0)
			{
				Flg = 0;
			}
		}

		for (int i = 0; i < 5; i++)
		{
			if ((world_model_->Opponents_[i] - ball_pos_).length() < 40.0)
			{
				Flg = 0;
			}
		}

		if (ball_mode == FLY && Flg == 1)
		{
			target = begin + (ball_vel_.length() / 26.0) * (ball_vel_.length() / 26.0) / (ball_vel_.length()) * ball_vel_;
			mode = 0; // init state
					  // ROS_INFO("target = (%.1lf, %.1lf)", target.x_, target.y_);
		}
		else if (ball_mode == FLY && Flg == 0)
		{
			target = DPoint(-10000.0, -10000.0); // useless
			mode = 0;							 // init state
		}
		else if (ball_mode == RUN)
		{
			target = DPoint(-10000.0, -10000.0); // useless
			mode = 0;							 // init state
		}
		else
		{
			target = DPoint(-10000.0, -10000.0); // useless
		}

		// ROS_INFO("ball_shoot_mode_ = %d", ball_mode);
	}
	else
	{
		target = DPoint(-10000.0, -10000.0); // useless
	}

	vel_change = ball_vel_.length();

	return target;
}

int Plan::oppDribble() // 0 - 4
{
	for (int i = 0; i < 5; ++i)
		if (world_model_->Opponents_[i].distance(ball_pos_) < 35.5)
			return i;
	return -1;
}

bool Plan::ball_is_free()
{
	for (int i = 0; i < 5; ++i)
	{
		if (world_model_->RobotInfo_[i].getDribbleState())
			return false;
	}
	return oppDribble() < 0;
}

int Plan::nearest_oppdribble()
{
	//??????????????????

	// auto opp_pos_ = world_model_->Opponents_[oppDribble()];
	double min = MAX;
	int min_id(0);
	for (int i = 1; i < 5; ++i) // exclude goalkeeper
	{
		if (defend_occupied[i])
			continue;
		auto rob_pos_ = world_model_->RobotInfo_[i].getLocation();
		if (rob_pos_.distance(ball_pos_) < min)
		{
			min = rob_pos_.distance(ball_pos_);
			min_id = i;
		}
	}
	defend_occupied[min_id] = 1;
	return min_id;
}

int Plan::numofinOurPenalty()
{
	int num(0);
	for (int i = 1; i < 5; ++i)
		if (world_model_->field_info_.isOurPenalty(world_model_->RobotInfo_[i].getLocation()))
			++num;
	return num;
}

int Plan::opp_nearestToOurGoal() // 1 - 4
{
	for (int i = 1; i < 5; ++i)
	{
		//??????????????????????????????????????????
		if (world_model_->field_info_.isOurPenalty(world_model_->Opponents_[i]))
			return i;
	}

	//????????????????????????
	auto goal = DPoint(-1100.0, 0.0);
	double min = MAX;
	int min_id(0);
	for (int i = 1; i < 5; ++i)
	{
		if (world_model_->Opponents_[i].distance(goal) < min)
		{
			min = world_model_->Opponents_[i].distance(goal);
			min_id = i;
		}
	}

	return min_id;
}

int Plan::Snd_nearest_oppdribble()
{
	auto opp_pos_ = world_model_->Opponents_[oppDribble()];
	double min = MAX;
	int min_id(0);

	int first = nearest_oppdribble();

	for (int i = 1; i < 5; ++i) // exclude goalkeeper
	{
		if (i == first)
			continue;
		auto rob_pos_ = world_model_->RobotInfo_[i].getLocation();
		if (rob_pos_.distance(opp_pos_) < min)
		{
			min = rob_pos_.distance(opp_pos_);
			min_id = i;
		}
	}
	return min_id;
}

// catch ball function , only passing ball
void Plan::CatchPassedBall()
{
	if (oppDribble() >= 0)
	{
		world_model_->pass_state_.reset();
		return;
	}

	action->handle_enable = 1;
	DPoint r2b = ball_pos_ - robot_pos_;

	action->rotate_acton = Positioned;
	action->move_action = No_Action;
	action->rotate_mode = 0;
	m_behaviour_.move2oriFAST(r2b.angle().radian_, robot_ori_.radian_);

	DPoint headoffPoint = robot_pos_;
	//?????????????????????

	if (ball_vel_.length())
	{
		action->move_action = CatchBall;
		double k1 = ball_vel_.y_ / ball_vel_.x_;
		double b1 = ball_pos_.y_ - k1 * ball_pos_.x_;
		double k2 = -1.0 / k1;
		double b2 = robot_pos_.y_ - k2 * robot_pos_.x_;
		headoffPoint = DPoint((b2 - b1) / (k1 - k2), (k1 * b2 - k2 * b1) / (k1 - k2)); //??????

		if (ball_pos_.x_ <= 0.005)
			headoffPoint = robot_pos_;

		if (m_behaviour_.move2target(headoffPoint, robot_pos_))
		{
			//??????
			if (robot_pos_.distance(ball_pos_) <= 37.2)
			{
				action->handle_enable = 1;
				action->move_action = CatchBall;
				action->rotate_acton = CatchBall;
				action->rotate_mode = 0;
				if (m_behaviour_.move2oriFAST(r2b.angle().radian_, robot_ori_.radian_, 0.087))
					m_behaviour_.move2target(ball_pos_, robot_pos_);
			}
		}
	}
}

// pass ball function
bool Plan::PassBall_Action(int catch_ID, int pass_mode_)
{

	// ROS_INFO("in pass fun %d", catch_ID);

	world_model_->pass_ID_ = world_model_->AgentID_;
	world_model_->catch_ID_ = catch_ID;
	world_model_->catch_pt_ = world_model_->RobotInfo_[catch_ID - 1].getLocation();
	bool shoot_flag = false;
	bool kick_off = false;

	// modify
	world_model_->is_passed_out_ = true;
	world_model_->pass_cmds_.isvalid = true;
	// m end
	DPoint pass_target_ = world_model_->RobotInfo_[catch_ID - 1].getLocation() - robot_pos_;
	if (m_behaviour_.move2oriFAST(pass_target_.angle().radian_, robot_ori_.radian_, 0.087))
	{
		world_model_->is_passed_out_ = true;
		world_model_->pass_cmds_.isvalid = true;
		world_model_->is_static_pass_ = 1; //???????????????????????????
		action->shootPos = pass_mode_;

		if (pass_mode_ == -1) // FLY
			action->strength = sqrt((pass_target_.length() / 100.0) * 9.8);
		else
		{
			action->strength = action->strength = pass_target_.length() / 30;
			if (action->strength < 5.0)
				action->strength = 5.0;
		}

		shoot_flag = true;
		std::cout << "pass out" << std::endl;
	}
	return shoot_flag;
}

bool Plan::penaltyOccupied()
{
	for (int i = 1; i < 5; ++i)
	{
		if(i == world_model_->AgentID_) continue;
		DPoint rob_pos = world_model_->RobotInfo_[i].getLocation();
		if (rob_pos.x_ > 875 && abs(rob_pos.y_) < 345){
			return true;
		}
	}
	return false;
	
}

void Plan::ProtectBallTry()
{
	double OppAngle[5];
	int i = 0, j = 0;
	for (DPoint op : world_model_->Opponents_)
	{
		DPoint tmp = op - robot_pos_;
		if (tmp.length() <= 150.0)
		{
			OppAngle[i] = tmp.angle().radian_;
			i++;
		}
	}
	double avg = 0.0;
	for (j = 0; j < i; j++)
	{
		avg += OppAngle[j];
	}
	avg /= i;
	m_behaviour_.move2oriFAST(avg + 180 * DEG2RAD, robot_ori_.radian_);
	action->move_action = MoveWithBall;
	action->rotate_acton = MoveWithBall;
	action->rotate_mode = 0;
}

void Plan::catchBall()
{
	auto r2b = ball_pos_ - robot_pos_;
	//action->handle_enable = 1;
	action->move_action = CatchBall;
	action->rotate_acton = CatchBall;
	action->rotate_mode = 0;

	if (robot_pos_.distance(ball_pos_) >= 500.0)
	{
		m_behaviour_.move2oriFAST(r2b.angle().radian_, robot_ori_.radian_, 0.087);
		m_behaviour_.move2target(ball_pos_, robot_pos_);
	}

	else
	{
		if (m_behaviour_.move2oriFAST(r2b.angle().radian_, robot_ori_.radian_, 0.087))
			m_behaviour_.move2targetFast(ball_pos_, robot_pos_);
	}
}

void Plan::moveAvoidBall(DPoint target)
{
	action->move_action = AvoidObs;
	action->rotate_acton = AvoidObs;
	action->rotate_mode = 0;

	m_behaviour_.movewithallObs(target, robot_pos_, 20.0, 5);
	m_behaviour_.move2oriFAST((ball_pos_ - robot_pos_).angle().radian_, robot_ori_.radian_);
}

bool Plan::moveBall(DPoint target)
{
	action->move_action = MoveWithBall;
	action->rotate_acton = MoveWithBall;
	action->rotate_mode = 0;

	//???????????????????????????
	if (m_behaviour_.move2oriFAST((target - robot_pos_).angle().radian_, robot_ori_.radian_))
	{
		m_behaviour_.move2targetk(target, robot_pos_, 20.0, 5);
	}

	if (check_my_dri_dis())
	{
		action->move_action = action->move_action = TurnForShoot;
		action->shootPos = 1;
		action->strength = 1.0f;
	}

	return (robot_pos_.distance(target) <= 25.0);
}

void Plan::shoot_1()
{
	const auto &goal_up = world_model_->field_info_.oppGoal_[GOAL_UPPER];
	const auto &goal_low = world_model_->field_info_.oppGoal_[GOAL_LOWER];
	static auto shoot_target = (world_model_->Opponents_[0].y_ > 0) ? goal_low : goal_up;

	action->move_action = TurnForShoot;
	action->rotate_acton = TurnForShoot;
	action->rotate_mode = 0;
	auto shoot_line = shoot_target - robot_pos_;

	m_behaviour_.move2oriFAST(shoot_line.angle().radian_, robot_ori_.radian_, 0.087, {0.0, 0.0}, 20.0, 100.0);

	//??????????????????????????????
	double t_x = robot_pos_.x_, t_y = robot_pos_.y_;
	auto k = tan(robot_ori_.radian_);
	double b = t_y - k * t_x;
	double y0 = 1100.0 * k + b;
	bool b1 = robot_ori_.radian_ / DEG2RAD >= -90.0 && robot_ori_.radian_ / DEG2RAD <= 90.0;

	if (b1 && y0 <= 95 && y0 >= -95 && fabs(y0 - world_model_->Opponents_[0].y_) >= 67.0)
	{
		action->shootPos = RUN;
		action->strength = 500;
		shoot_flag = true;
		std::cout << "shoot done " << std::endl;

		return;
	}

	else
	{
		
	}

	if (fabs(shoot_line.angle().radian_ - robot_ori_.radian_) <= 0.10)
	{
		shoot_target = (shoot_target == goal_up) ? goal_low : goal_up;
	}
}

void Plan::shoot_2()
{
	const auto &goal_up = world_model_->field_info_.oppGoal_[GOAL_UPPER];
	const auto &goal_low = world_model_->field_info_.oppGoal_[GOAL_LOWER];
	static auto shoot_target = (world_model_->Opponents_[0].y_ > 0) ? goal_low : goal_up;

	action->move_action = TurnForShoot;
	action->rotate_acton = TurnForShoot;
	action->rotate_mode = 0;
	auto shoot_line = shoot_target - robot_pos_;

	m_behaviour_.move2oriFAST(shoot_line.angle().radian_, robot_ori_.radian_, 0.087, {0.0, 0.0}, 20.0, 100.0);

	//??????????????????????????????
	double t_x = robot_pos_.x_, t_y = robot_pos_.y_;
	auto k = tan(robot_ori_.radian_);
	double b = t_y - k * t_x;
	double y0 = 1100.0 * k + b;
	bool b1 = robot_ori_.radian_ / DEG2RAD >= -90.0 && robot_ori_.radian_ / DEG2RAD <= 90.0;

	while (b1 && y0 <= 95 && y0 >= -95 && fabs(y0 - world_model_->Opponents_[0].y_) >= 70.0)
	{
		t_x = robot_pos_.x_, t_y = robot_pos_.y_;
		k = tan(robot_ori_.radian_);
		b = t_y - k * t_x;
		y0 = 1100.0 * k + b;
		b1 = robot_ori_.radian_ / DEG2RAD >= -90.0 && robot_ori_.radian_ / DEG2RAD <= 90.0;

		action->shootPos = RUN;
		action->strength = 500;
		shoot_flag = true;
		std::cout << "shoot done " << std::endl;

		return;
	}
	
	if (fabs(shoot_line.angle().radian_ - robot_ori_.radian_) <= 0.10)
	{
		shoot_target = (shoot_target == goal_up) ? goal_low : goal_up;
	}
}
//??????????????? ??????????????????????????????
void Plan::lob()
{
	action->move_action = TurnForShoot;
	action->rotate_acton = TurnForShoot;
	action->rotate_mode = 0;

	DPoint tmp = DPoint(1100.0, 0.0);

	if (m_behaviour_.move2oriFAST((tmp - robot_pos_).angle().radian_, robot_ori_.radian_))
	{
		action->shootPos = -1;
		action->strength = sqrt((((tmp - robot_pos_).length() / 100.0) + 1.2) * 9.8);
		shoot_flag = true;
		std::cout << "shoot done " << std::endl;
	}
}

int Plan::canPass(int catchID)
{
	DPoint catch_pos_ = world_model_->RobotInfo_[catchID - 1].getLocation();
	if (catchID && catchID != world_model_->AgentID_ && isInField(catch_pos_, 10.0, 10.0))
	{

		DPoint p2c = catch_pos_ - robot_pos_;
		// int ret = 1;
		int fix(0);
		for (int i = 0; i < 5; ++i)
		{
			DPoint obs_ = world_model_->Opponents_[i];
			DPoint p2obs = obs_ - robot_pos_;
			DPoint obs2c = catch_pos_ - obs_;

			//????????????pass ??? catch??????
			if (p2c.ddot(p2obs) > 0 && (p2c.ddot(p2obs) / p2c.length()) < p2c.length() && p2c.ddot(p2obs) / (p2obs.length() * p2c.length()) > 0.7)
			{
				//??????????????????100;
				// if (p2c.cross(p2obs) / p2c.length() < 100.0)
				// {
				// 	auto k = p2c.length() / (p2c.ddot(p2obs) / p2c.length());
				// 	//??????
				// 	if (k <= 2.5 && k >= 1.67)
				// 		ret = -1;

				// 	else
				// 		return 0;
				// }

				fix = 1;
			}
		}
		if (!fix)
			return 1; // RUN

		fix = 0;

		if (catch_pos_.distance(robot_pos_) > 700.0)
		{
			for (int i = 0; i < 5; i++)
			{
				DPoint obs_ = world_model_->Opponents_[i];
				DPoint p2obs = obs_ - robot_pos_;
				DPoint obs2c = catch_pos_ - obs_;
				double cos = p2c.ddot(p2obs) / (p2c.length() * p2obs.length());
				if (cos > 1.73 / 2 && p2obs.length() < 200.0)
				{
					fix = 1;
				}
			}
			if (!fix)
				return -1; // FLY
		}
		else
			return 0;
	}
	else
		return 0;

	return 0;
}

void Plan::defend1v1()
{

	///????????????

	DPoint opp_dribbling_pos_ = world_model_->Opponents_[oppDribble()];
	// DPoint opp2b = ball_pos_ - opp_dribbling_pos_;
	// Angle oppd_ori_ = opp2b.angle(); // ????????????????????????

	auto target = opp_dribbling_pos_.pointofline(ball_pos_, 150.0);
	auto r2b = ball_pos_ - robot_pos_;
	auto r2opp = opp_dribbling_pos_ - robot_pos_;
	int att = oppDribble();
	DPoint target_;
	if (!ball_is_free())
	{
		// action->move_action = CatchBall;
		// action->rotate_acton = CatchBall;
		// action->rotate_mode = 0;
		// m_behaviour_.move2oriFAST(r2b.angle().radian_, robot_ori_.radian_);
		// m_behaviour_.move2target(target, robot_pos_);
		DPoint opp_vel_ = get_opp_vel(att);
		double opp_ori_ = get_opp_ori(att);
		predict_attackerwithball(att);
		target_ = ball_pos_;
		DPoint goal(-1100, 0);
		DPoint a2goal = goal - attacker_next_pos;
		double angle_cos = a2goal.ddot(a2b_next) / (a2goal.length() * a2b_next.length());
		if (angle_cos > 0.5)
		{
			target_ = attacker_next_pos + 55 * a2b_next;
		}
		else
		{
			if (a2goal.cross(a2b_next) > 0)
			{
				DPoint ver(-a2goal.y_, a2goal.x_);
				double l = ver.length();
				double l2 = a2goal.length();
				target_ = (27.5 * 0.75 / l2) * a2goal + (47.63 * 0.75 / l) * ver + attacker_next_pos;
			}
			else if (a2goal.cross(a2b_next) < 0)
			{
				DPoint ver(a2goal.y_, -a2goal.x_);
				double l = ver.length();
				double l2 = a2goal.length();
				target_ = (27.5 * 0.75 / l2) * a2goal + (47.63 * 0.75 / l) * ver + attacker_next_pos;
			}
		}

		action->handle_enable = 1;
		action->move_action = CatchBall;
		action->rotate_acton = CatchBall;
		action->rotate_mode = 0;
		if (m_behaviour_.move2target(target_, robot_pos_))
			m_behaviour_.move2oriFAST(r2b.angle().radian_, robot_ori_.radian_);
	}

	else if (r2b.ddot(r2opp) > 0) //?????????????????????
	{
		// try to block

		// move to trial vertically
		DPoint headoffPoint = robot_pos_;
		double k1 = ball_vel_.y_ / ball_vel_.x_;
		double b1 = ball_pos_.y_ - k1 * ball_pos_.x_;
		double k2 = -1.0 / k1;
		double b2 = robot_pos_.y_ - k2 * robot_pos_.x_;
		headoffPoint = DPoint((b2 - b1) / (k1 - k2), (k1 * b2 - k2 * b1) / (k1 - k2));

		if (m_behaviour_.move2target(headoffPoint, robot_pos_))
		{
			//??????
			if (robot_pos_.distance(ball_pos_) <= 37.2 )
			{
				action->handle_enable = 1;
				action->move_action = CatchBall;
				action->rotate_acton = CatchBall;
				action->rotate_mode = 0;
				if (m_behaviour_.move2oriFAST(r2b.angle().radian_, robot_ori_.radian_, 0.087))
					m_behaviour_.move2target(ball_pos_, robot_pos_);
			}
		}
	}
}

int Plan::nearest_point(DPoint point)
{
	// int occupied = nearest_oppdribble();

	double min = MAX;
	int min_id;
	for (int i = 1; i < 5; ++i)
	{
		if (defend_occupied[i])
			continue;
		auto rob_pos = world_model_->RobotInfo_[i].getLocation();
		if (rob_pos.distance(point) < min)
		{
			min = rob_pos.distance(point);
			min_id = i;
		}
	}
	defend_occupied[min_id] = 1;
	return min_id;
}

int Plan::nearest_opp(int exp1, int exp2)
{
	int min = MAX;
	int min_id(-1);
	for (int i = 1; i < 5; ++i)
	{
		if (i == exp1 || i == exp2)
			continue;
		if (robot_pos_.distance(world_model_->Opponents_[i]) < min)
		{
			min = robot_pos_.distance(world_model_->Opponents_[i]);
			min_id = i;
		}
	}

	return min_id;
}

void Plan::block()
{
	DPoint r2b = ball_pos_ - robot_pos_;
	//??????????????????????????????????????????
	DPoint opp_pos1 = world_model_->Opponents_[opp_nearestToOurGoal()];
	//???????????????????????????
	DPoint opp_pos2 = world_model_->Opponents_[oppDribble()];

	DPoint target;
	if (opp_pos1.distance(ball_pos_) <= 100.0)
		target = ball_pos_.pointofline(opp_pos1, 100.0);
	else
		target = opp_pos1.pointofline(opp_pos2, 100.0);

	action->handle_enable = 1;
	action->move_action = CatchBall;
	action->rotate_acton = CatchBall;
	action->rotate_mode = 0;
	m_behaviour_.move2oriFAST(r2b.angle().radian_, robot_ori_.radian_);
	m_behaviour_.move2target(target, robot_pos_);

	return;
}

void Plan::mark()
{
	int opp_dri_ = oppDribble();
	DPoint opp_pos2 = world_model_->Opponents_[opp_dri_];
	int opp_near_goal_ = opp_nearestToOurGoal();
	double ori = (ball_pos_ - world_model_->Opponents_[opp_dri_]).angle().radian_;
	DPoint r2b = ball_pos_ - robot_pos_;
	DPoint target;
	bool Flg = 0;
	int pass_id_ = -1;

	//??????????????????????????????

	for (int i = 1; i < 5; ++i)
	{
		if (i == opp_dri_ || i == opp_near_goal_)
			continue;
		double ori2 = (world_model_->Opponents_[i] - world_model_->Opponents_[opp_dri_]).angle().radian_;
		if (fabs(ori - ori2) <= 15.0 / DEG2RAD)
		{
			Flg = 1;
			pass_id_ = i;
		}
	}

	if (Flg)
	{
		//?????????????????????
		auto opp_pos_ = world_model_->Opponents_[pass_id_];
		target = opp_pos_.pointofline(opp_pos2, 125.0);
	}

	else //?????????????????????????????????
	{
		//??????????????????????????????????????????
		DPoint opp_pos1 = world_model_->Opponents_[nearest_opp(opp_dri_, opp_near_goal_)];

		// if(opp_pos1.distance(opp_pos2) <= 200.0) return;

		DPoint target = opp_pos1.pointofline(opp_pos2, 125.0);
	}

	action->handle_enable = 1;
	action->move_action = CatchBall;
	action->rotate_acton = CatchBall;
	action->rotate_mode = 0;
	m_behaviour_.move2oriFAST(r2b.angle().radian_, robot_ori_.radian_);
	m_behaviour_.move2target(target, robot_pos_);

	return;
}

int Plan::opp_getsforward()
{
	int min = MAX;
	int min_id(-1);
	for (int i = 1; i < 5; ++i)
	{
		if (world_model_->Opponents_[i].x_ < min)
		{
			min = world_model_->Opponents_[i].x_;
			min_id = i;
		}
	}
	return min_id;
}

double Plan::k2(const DPoint &p1, const DPoint &p2)
{
	return ((p1.y_ - p2.y_) / (p1.x_ / p2.x_));
}

void Plan::defend_shoot()
{
	DPoint target;
	DPoint opp_pos_ = opp_sort[0]; // ???????????????????????????
	if (opp_pos_.x_ <= -875)
	{
		target = ball_pos_.pointofline(opp_pos_, 75.0);
	}
	else
	{
		target.x_ = -1000.0;
		double k = k2(ball_pos_, opp_pos_);
		target.y_ = k * (-1000.0) + ball_pos_.y_ - k * ball_pos_.x_;
	}

	action->handle_enable = 1;
	action->move_action = CatchBall;
	action->rotate_acton = CatchBall;
	action->rotate_mode = 0;
	m_behaviour_.move2oriFAST((ball_pos_ - robot_pos_).angle().radian_, robot_ori_.radian_);
	m_behaviour_.move2targetFast(target, robot_pos_);
}

void Plan::blockPassingBall()
{
	//???????????????????????????????????????
	DPoint opp_pos_ = world_model_->Opponents_[opp_getsforward()];
	// DPoint opp_pos_ = world_model_->Opponents_[opp_getsforward()]; //??????

	DPoint target = opp_pos_.pointofline(ball_pos_, 150.0);

	action->handle_enable = 1;
	action->move_action = CatchBall;
	action->rotate_acton = CatchBall;
	action->rotate_mode = 0;
	m_behaviour_.move2oriFAST((ball_pos_ - robot_pos_).angle().radian_, robot_ori_.radian_);
	m_behaviour_.move2target(target, robot_pos_);
}

void Plan::update_opp_sort()
{
	for (int i = 0; i < 4; ++i)
		opp_sort[i] = world_model_->Opponents_[opp_near_ball_sort_ID[i] - 1];
}

// ??????????????????????????????????????? ??? ?????????????????????????????????
void Plan::update_near()
{
	double minlength = MAX;
	for (int i = 2; i <= 5; i++)
	{
		if (!ifParking(world_model_->RobotInfo_[i - 1].getLocation()) && i != our_near_ball_sort_ID[0]) // except our robot which is the nearest to ball
		{
			if ((world_model_->RobotInfo_[i - 1].getLocation() - opp_goal).length() < minlength)
			{
				our_nearest_oppgoal_ID = i;
				minlength = (world_model_->RobotInfo_[i - 1].getLocation() - opp_goal).length();
			}
		}
	}

	minlength = MAX;

	for (int i = 2; i <= 5; i++)
	{
		if (!ifParking(world_model_->RobotInfo_[i - 1].getLocation()) && i != our_near_ball_sort_ID[0]) // except our robot which is the nearest to ball
		{
			if ((world_model_->RobotInfo_[i - 1].getLocation() - our_goal).length() < minlength)
			{
				our_nearest_ourgoal_ID = i;
				minlength = (world_model_->RobotInfo_[i - 1].getLocation() - our_goal).length();
			}
		}
	}
}

void Plan::update_a()
{
	update_past_velocity();
	update_opp_location();
	update_a2b_radian();
	sortourID();
	sortoppID();
	update_opp_sort();
	update_near();
	landing_pos_ = landing_pos();
	ispenaltyOccupied = penaltyOccupied();
}

void Plan::pre_attack()
{
	DPoint pre_attack_pos_(500.0, 0.0);
	auto our_nearest_ball_pos = world_model_->RobotInfo_[our_near_ball_sort_ID[0] - 1].getLocation();
	if (ball_pos_.x_ - our_nearest_ball_pos.x_ != 0.0)
	{
		pre_attack_pos_.y_ = ball_pos_.y_ + (500.0 - ball_pos_.x_) * (ball_pos_.y_ - our_nearest_ball_pos.y_) / (ball_pos_.x_ - our_nearest_ball_pos.x_);
	}
	pre_attack_pos_.y_ = std::max(-600.0, pre_attack_pos_.y_);
	pre_attack_pos_.y_ = std::min(600.0, pre_attack_pos_.y_);
	int can_goto_point = 1;
	DPoint need_defend_pos;
	for (int i = 0; i < 5; i++)
	{
		if ((world_model_->Opponents_[i] - pre_attack_pos_).length() < 200.0)
		{
			can_goto_point = 0;
			need_defend_pos = world_model_->Opponents_[i];
			break;
		}
	}
	DPoint target;
	if (can_goto_point == 1) // can go to that point
		target = pre_attack_pos_;
	else // can not go to that point
	{
		target = need_defend_pos.pointofline(ball_pos_, 100.0);
	}

	auto r2b = ball_pos_ - robot_pos_;

	action->handle_enable = 1;
	action->move_action = CatchBall;
	action->rotate_acton = CatchBall;
	action->rotate_mode = 0;
	if (m_behaviour_.move2target(target, robot_pos_))
		m_behaviour_.move2oriFAST(r2b.angle().radian_, robot_ori_.radian_);
}

void Plan::pre_attack_2()
{
	DPoint target;
	target = world_model_->assist_pt_;
	auto r2b = ball_pos_ - robot_pos_;

	action->handle_enable = 1;
	action->move_action = CatchBall;
	action->rotate_acton = CatchBall;
	action->rotate_mode = 0;
	if (m_behaviour_.move2target(target, robot_pos_))
		m_behaviour_.move2oriFAST(r2b.angle().radian_, robot_ori_.radian_);
}

void Plan::defend_point(DPoint target_pos_)
{
	DPoint target;
	auto r2b = ball_pos_ - robot_pos_;
	if (target_pos_.distance(ball_pos_) <= 150.0)
		target = ball_pos_.pointofline(target_pos_, 100.0);
	else
		target = target_pos_.pointofline(ball_pos_, 75.0);

	action->handle_enable = 1;
	action->move_action = CatchBall;
	action->rotate_acton = CatchBall;
	action->rotate_mode = 0;
	if (m_behaviour_.move2target(target, robot_pos_))
		m_behaviour_.move2oriFAST(r2b.angle().radian_, robot_ori_.radian_);
}

void Plan::move2landingPos()
{
	auto r2b = ball_pos_ - robot_pos_;
	action->handle_enable = 1;
	action->move_action = CatchBall;
	action->rotate_acton = CatchBall;
	action->rotate_mode = 0;
	if (m_behaviour_.move2targetFast(landing_pos_, robot_pos_)) //?????????
		m_behaviour_.move2oriFAST(r2b.angle().radian_, robot_ori_.radian_);
}

void Plan::move2catch(DPoint target)
{
	auto r2b = ball_pos_ - robot_pos_;
	action->handle_enable = 1;
	action->move_action = CatchBall;
	action->rotate_acton = CatchBall;
	action->rotate_mode = 0;
	if (m_behaviour_.move2targetFast(target, robot_pos_)) //?????????
		m_behaviour_.move2oriFAST(r2b.angle().radian_, robot_ori_.radian_);
}

void Plan::defend()
{
	update_a();

	//????????????
	if (world_model_->pass_state_.catch_id_ != -1)
	{

		if (world_model_->pass_state_.catch_id_ == world_model_->AgentID_)
		{
			// ROS_INFO("catch passedball");
			CatchPassedBall();
			// DPoint catch_point;
			// if (landing_pos_.x_ > -9000.0)
			// { //?????????????????????????????????
			// 	catch_point = landing_pos_;
			// }
			// else
			// 	catch_point = ball_pos_ + (ball_vel_) * ((ball_pos_ - robot_pos_).length() / ball_vel_.length()); //????????????????????????????????????
			// action->move_action = CatchBall;
			// action->rotate_acton = CatchBall;
			// action->rotate_mode = 0;
			// action->handle_enable = 1;
			// m_behaviour_.move2oriFAST((ball_pos_ - robot_pos_).angle().radian_, robot_ori_.radian_, 0.0, ball_pos_, 11.0);
			// m_behaviour_.move2targetk(catch_point, robot_pos_, 20.0, 5);
		}
		else
		{
			attack();
		}
	}
	else
	{
		if (oppDribble() != -1) //????????????
		{
			int opp_dri = oppDribble(), opp_near_goal = opp_nearestToOurGoal();
			auto opp_dri_pos = (opp_dri >= 0) ? (world_model_->Opponents_[opp_dri]) : DPoint(0.0, 0.0);
			auto opp_near_goal_pos = world_model_->Opponents_[opp_near_goal];

			if (ball_pos_.x_ <= 0) //??????????????????
			{
				//????????????????????????
				if (opp_dri == opp_near_goal)
				{
					if (world_model_->AgentID_ - 1 == nearest_oppdribble())
					{
						defend_shoot();
					}

					else
					{
						mark();
					}
				}

				//???????????????????????????
				else
				{
					DPoint target = opp_near_goal_pos.pointofline(opp_dri_pos, 150.0);
					//????????????????????????????????????
					if (world_model_->AgentID_ - 1 == nearest_oppdribble())
					{
						// 1-1??????
						defend1v1();
					}

					else if (world_model_->AgentID_ - 1 == nearest_point(target))
					{
						//??????????????????????????????
						block();
					}

					else
					{
						//??????????????????????????????
						int avail[2];
						for (int i = 1, j = 0; i < 5; ++i)
						{
							if (defend_occupied[i] == 0)
								avail[j++] = i;
						}

						if (world_model_->RobotInfo_[avail[0]].getLocation().x_ > world_model_->RobotInfo_[avail[1]].getLocation().x_)
						{
							std::swap(avail[0], avail[1]);
						}

						if (world_model_->AgentID_ - 1 == avail[0])
						{
							//?????????????????????????????????????????????????????????
							mark();
						}

						else
						{
							//????????????????????????????????????
							pre_attack();
						}
						// mark();
					}
				}
			}

			else //??????????????????
			{
				if (world_model_->AgentID_ - 1 == nearest_oppdribble())
				{
					defend1v1();
				}

				else if (world_model_->AgentID_ != our_nearest_ourgoal_ID)
				{
					//????????????????????????
					int avail[2];
					for (int i = 1, j = 0; i < 5; ++i)
					{
						if (i != our_nearest_ourgoal_ID - 1 && defend_occupied[i] == 0)
							avail[j++] = i;
					}
					//???????????????????????????????????????
					auto opp_pos_ = world_model_->Opponents_[opp_near_ball_sort_ID[1] - 1];

					if (world_model_->RobotInfo_[avail[0]].getLocation().distance(opp_pos_) > world_model_->RobotInfo_[avail[1]].getLocation().distance(opp_pos_))
					{
						std::swap(avail[0], avail[1]);
					}

					if (world_model_->AgentID_ - 1 == avail[0])
					{
						defend_point(opp_pos_);
					}
					else
					{
						defend_point(world_model_->Opponents_[opp_near_ball_sort_ID[2] - 1]);
					}
				}

				//?????????????????????
				else if (world_model_->AgentID_ == our_nearest_ourgoal_ID)
				{
					DPoint target;
					int Flg = 0;
					int defend_ID = -1;
					double minlength2 = MAX;
					for (int i = 2; i <= 5; ++i)
					{
						if (world_model_->Opponents_[i - 1].x_ < 0.0 && (world_model_->Opponents_[i - 1] - our_goal).length() < minlength2)
						{
							if (world_model_->Opponents_[i - 1].y_ > -700.0 && world_model_->Opponents_[i - 1].y_ < 700.0)
							{
								Flg = 1;
								defend_ID = i;
								minlength2 = (world_model_->Opponents_[i - 1] - our_goal).length();
							}
						}
					}

					if (Flg == 0) // there is no opprobot in our semi-ground
					{
						target.x_ = 0.0;
						target.y_ = ball_pos_.y_;
					}
					else // there is one or more than one opprobot in our semi-ground, then prevent the nearest one to our goal
					{
						target = world_model_->Opponents_[defend_ID - 1] + 100.0 / ((ball_pos_ - world_model_->Opponents_[defend_ID - 1]).length()) * (ball_pos_ - world_model_->Opponents_[defend_ID - 1]);
					}
					move2catch(target);
				}
			}
		}

		else // ball's free
		{
			if (ball_pos_.x_ <= 0) //??????????????????
			{
				if (world_model_->field_info_.isOurPenalty(robot_pos_))
				{
					catchBall();
				}
				else
				{
					DPoint target;
					//???????????????????????????
					if (world_model_->AgentID_ == our_near_ball_sort_ID[0])
					{
						if (landing_pos_.x_ > -1200) // ball is flying
						{
							move2landingPos();
							catchBall();
						}
						else
						{
							catchBall();
						}
					}

					else if (world_model_->AgentID_ != our_nearest_oppgoal_ID)
					{
						//???????????????
						//??????
						int avail[2];
						for (int i = 1, j = 0; i < 5; ++i)
						{
							if (i != our_near_ball_sort_ID[0] - 1 && i != our_nearest_oppgoal_ID - 1)
								avail[j++] = i;
							// useless
							if (j == 2)
								break;
						}

						auto opp_pos_ = opp_sort[1]; //??????????????????????????????
						if (world_model_->RobotInfo_[avail[0]].getLocation().distance(opp_pos_) > world_model_->RobotInfo_[avail[1]].getLocation().distance(opp_pos_))
						{
							std::swap(avail[0], avail[1]);
						}
						if (world_model_->AgentID_ - 1 == avail[0])
						{
							defend_point(opp_sort[1]);
						}
						else
						{
							defend_point(opp_sort[2]);
						}
					}
					else if (world_model_->AgentID_ == our_nearest_oppgoal_ID)
					{
						pre_attack();
					}
				}
			}
			else //??????????????????
			{
				if (world_model_->field_info_.isOppPenalty(robot_pos_))
				{
					catchBall();
				}
				else
				{
					if (world_model_->AgentID_ == our_near_ball_sort_ID[0])
					{
						catchBall();
					}

					else if (world_model_->AgentID_ != our_nearest_ourgoal_ID)
					{
						int avail[2];
						for (int i = 1, j = 0; i < 5; i++)
						{
							if (i != our_near_ball_sort_ID[0] - 1 && i != our_nearest_ourgoal_ID - 1)
							{
								avail[j++] = i;
							}
						}
						auto opp_pos_ = opp_sort[1];
						if (world_model_->RobotInfo_[avail[0]].getLocation().distance(opp_pos_) > world_model_->RobotInfo_[avail[1]].getLocation().distance(opp_pos_))
						{
							std::swap(avail[0], avail[1]);
						}

						if (world_model_->AgentID_ == avail[0])
						{
							defend_point(opp_pos_);
						}
						else
						{
							defend_point(opp_sort[2]);
						}
					}

					else //?????????????????????
					{
						DPoint target;
						int Flg = 0;
						int defend_ID = 0;
						double minlength2 = 10000.0;
						for (int i = 2; i <= 5; i++)
						{
							if (world_model_->Opponents_[i - 1].x_ < 0.0 && (world_model_->Opponents_[i - 1] - our_goal).length() < minlength2)
							{
								if (world_model_->Opponents_[i - 1].y_ > -700.0 && world_model_->Opponents_[i - 1].y_ < 700.0)
								{
									Flg = 1;
									defend_ID = i;
									minlength2 = (world_model_->Opponents_[i - 1] - our_goal).length();
								}
							}
						}

						if (Flg == 0)
						{
							target = DPoint(0.0, ball_pos_.y_);
						}
						else
						{
							target = world_model_->Opponents_[defend_ID - 1] + 100.0 / ((ball_pos_ - world_model_->Opponents_[defend_ID - 1]).length()) * (ball_pos_ - world_model_->Opponents_[defend_ID - 1]);
						}
						move2catch(target);
					}
				}
			}
		}
	}
	return;
}

//............................................................................//

int Plan::dribble_dis() //???????????????????????????
{
	static bool startDribbling = false;
	static bool dribbleWarning = false;
	static DPoint start_dri_pos(0.0, 0.0);
	double dribble_dis;

	if (world_model_->RobotInfo_[world_model_->AgentID_ - 1].getDribbleState())
	{
		if (startDribbling == false)
		{
			start_dri_pos = robot_pos_;
			startDribbling = true;
			dribbleWarning = false;
		}
		else
		{
			return (robot_pos_ - start_dri_pos).length();
		}
	}
	else
	{
		startDribbling = false;
		dribbleWarning = false;
		start_dri_pos = robot_pos_;
		return 0;
	}
}

bool Plan::isInField(DPoint world_pt, double expand_len, double expand_width) //????????????????????????????????????
{
	return (world_pt.x_ > -1100.0 + expand_len &&
			world_pt.x_ < 1100.0 - expand_len &&
			world_pt.y_ > -700.0 + expand_width &&
			world_pt.y_ < 700.0 - expand_width);
}

int Plan::can_pass_near_goal(int &mode)
{
	int min = robot_pos_.distance(opp_goal);
	int receiver = -1;
	for (int i = 2; i <= 5; ++i)
	{
		int pass_mode = canPass(i);
		if (robot_pos_.distance(world_model_->RobotInfo_[i - 1].getLocation()) > 100.0 && pass_mode && world_model_->RobotInfo_[i - 1].getLocation().distance(opp_goal) < min)
		{
			receiver = i;
			mode = pass_mode;
		}
	}
	return receiver;
}

void Plan::fly_shoot()
{
	auto tmp = DPoint(1100.0, 0.0);
	if (m_behaviour_.move2oriFAST((opp_goal - robot_pos_).angle().radian_, robot_ori_.radian_))
	{
		action->shootPos = FLY;
		action->strength = sqrt((((tmp - robot_pos_).length() / 100.0) + 1.2) * 9.8);
		shoot_flag = true;
	}
}

bool Plan::isWinger(const DPoint pos_)
{
	return (pos_.y_ >= 400.0 || pos_.y_ <= -400.0);
}

bool Plan::is1v1(const DPoint pos_)
{
	for (int i = 1; i < 5; ++i)
	{
		if (world_model_->Opponents_[i].x_ > pos_.x_)
			return false;
	}

	return true;
}

bool Plan::check_my_dri_dis() //???????????????????????????
{
	static bool startDribbling = false;
	static bool dribbleWarning = false;
	static DPoint start_dri_pos(0.0, 0.0);

	if (world_model_->RobotInfo_[world_model_->AgentID_ - 1].getDribbleState())
	{
		if (startDribbling == false)
		{
			start_dri_pos = robot_pos_;
			startDribbling = true;
			dribbleWarning = false;
		}
		else
		{
			if ((robot_pos_ - start_dri_pos).length() > 250.0)
			{
				dribbleWarning = true;
			}
		}
	}
	else
	{
		startDribbling = false;
		dribbleWarning = false;
		start_dri_pos = robot_pos_;
	}
	return dribbleWarning;
}

void Plan::cross()
{
	vector<int> attacker_IDs;
	for (int i = 2; i <= 5; ++i)
	{
		if (i == world_model_->AgentID_)
			continue;
		if (i != our_nearest_ourgoal_ID)
			attacker_IDs.push_back(i);
	}

	// 0 ?????????????????????
	// ??????????????????????????????????????????
	if (world_model_->RobotInfo_[attacker_IDs[0] - 1].getLocation().x_ < world_model_->RobotInfo_[attacker_IDs[1] - 1].getLocation().x_)
	{
		std::swap(attacker_IDs[0], attacker_IDs[1]);
	}

	int pass_mode = canPass(attacker_IDs[0]);
	if (pass_mode)
	{
		PassBall_Action(attacker_IDs[0], pass_mode);
		return;
	}
	pass_mode = canPass(attacker_IDs[1]);
	if (pass_mode)
	{
		PassBall_Action(attacker_IDs[1], pass_mode);
		return;
	}

	//????????????????????? 

	pass2others();
}

bool Plan::canDribble(const DPoint target)
{
	//????????????????????????????????????
	auto opp_pos1_ = opp_sort[0];
	auto vec1 = target - robot_pos_;
	auto vec2 = opp_pos1_ - robot_pos_;
	double length = fabs(vec1.cross(vec2)) / (vec1.length() * vec2.length());
	return (length >= 150.0);
}

bool Plan::isInOurGround(){
	for(int i = 0; i < 5; i++)
	{
		if(world_model_->Opponents_[i].x_ < 0.0)
		{
			return true;
		}
	}
	return false;
}

void Plan::pass2others()
{
	for(int i = 5; i >= 1; --i)
	{
		if(i == world_model_->AgentID_ - 1)
			continue;
		
		int mode = canPass(i);
		if(mode)
		{
			PassBall_Action(i, mode);
		}
	}
}

void Plan::attack()
{
	update_a();
	static int enterFrom = NEVER;
	if (world_model_->RobotInfo_[world_model_->AgentID_ - 1].getDribbleState())
	{
		//??????????????????

		//?????????????????????
		if (ball_pos_.x_ <= 0)
		{
			if (is1v1(robot_pos_)) //????????????
			{
				moveBall(DPoint(900.0, 0.0));
			}

			else
			{
				//???????????????????????????
				int mode = canPass(our_nearest_oppgoal_ID);
				if (mode)
				{
					PassBall_Action(our_nearest_oppgoal_ID, mode);
				}
				else
				{
					//??????    ??????????????????
					vector<int> wingersID;
					for (int i = 2; i <= 5; ++i)
					{
						if (i == world_model_->AgentID_)
							continue;
						if (isWinger(world_model_->RobotInfo_[i].getLocation()))
						{
							wingersID.push_back(i);
						}
					}

					if (wingersID.empty())
					{
						//?????????????????????
						//?????????????????????

						pass2others();
					}

					else
					{
						for (int id : wingersID)
						{
							int mode = canPass(id);
							if (mode)
							{
								PassBall_Action(id, mode);
								return;
							}

							pass2others();
						}
					}
				}
			}
		}
		else //?????????????????????
		{
			//?????????????????? ????????????
			if (opp_goal.distance(robot_pos_) <= 410.0)
			{
				shoot_2();
			}

			else
			{
				if (is1v1(robot_pos_)) //??????
				{
					moveBall(DPoint(900.0, 0.0));
				}

				else
				{
					//??????????????????
					if (isWinger(robot_pos_))
					{

						if (0)
						{
							//????????????
						}
						else
						{
							//???????????????????????????   ??????
							if (robot_pos_.x_ <= 750.0)
							{
								moveBall(DPoint(760.0, robot_pos_.y_));
							}
							else
							{
								//??????
								cross();
							}
						}
					}
					//??????????????????
					else
					{
						//????????????????????????
						if (canDribble(DPoint(900.0, 0.0)))
						{
							//????????????
							moveBall(DPoint(900.0, 0.0));
						}
						else
						{
							//???????????? ??????????????????
							vector<int> wingersID;
							for (int i = 2; i <= 5; ++i)
							{
								if (i == world_model_->AgentID_)
									continue;
								if (isWinger(world_model_->RobotInfo_[i].getLocation()))
								{
									wingersID.push_back(i);
								}
							}

							if (wingersID.empty())
							{
								//?????????????????????
								//?????????????????????????????? ?????????????????????
								lob();
							}
							else
							{
								for (int id : wingersID)
								{
									int mode = canPass(id);
									if(mode)
									{
										PassBall_Action(id, mode);
									}
								}

								//???????????????
								pass2others();
							}
						}
					}
				}
			}
		}
	}

	//??????????????????
	else
	{
			
			//enterFrom???????????????????????? SIDE MID NERVER
			enterFrom = NEVER;
			int our_dribble = ourDribble();

			//??????????????????
			if (ball_pos_.x_ < 0.0)
			{
				DPoint tar_pos_(0.0,0.0);
                DPoint mid_goal = DPoint(1100.0,0.0);

                if(world_model_->AgentID_ == our_nearest_oppgoal_ID)//???????????????????????????
				{
					pre_attack_2();
				}
				else
				{
					//?????? ???????????? ???????????? ??????????????????????????? ??????????????????????????????????????????????????????
					for(int i = 2, j = 0; i <= 5; i++)
                    {
                        if(i != our_dribble && i != our_nearest_oppgoal_ID)
                        {
                            available_ID[j] = i;
                            j++;
                        }
                    }
					if((world_model_->RobotInfo_[available_ID[0]-1].getLocation()-our_goal).length() < (world_model_->RobotInfo_[available_ID[1]-1].getLocation() - our_goal).length())
					{
						std::swap(available_ID[0],available_ID[1]);
					}
					// ????????????????????????????????????
					if(world_model_->AgentID_ == available_ID[0])
					{	
						DPoint target;

						// if(isInOurGround())
						// {
						// 	int opp_near_goal = opp_nearestToOurGoal();
						// 	DPoint opp_danger_pos = world_model_->Opponents_[opp_near_goal];

						// 	target = opp_danger_pos.pointofline(opp_danger_pos, 125.0);

						// }
						// else
						// {
						// 	if(world_model_->RobotInfo_[our_dribble-1].getLocation().y_ > 0)
						// 	{
						// 		target = DPoint(-100,350);
						// 	}
						// 	else 
						// 	{
						// 		target = DPoint(-100,-350);
						// 	}
						// }
						tar_pos_ = world_model_->pass_pt_;
						action->handle_enable = 1;
						action->move_action = CatchBall;
						action->rotate_acton = CatchBall;
						action->rotate_mode = 0;
						m_behaviour_.move2target(target, robot_pos_);
					}
					// // ?????????????????????????????? ??????
					// if(world_model_->AgentID_ == available_ID[0])
					// {
					// 	if((opp_sort[1] - ball_pos_).length() < 150)
					// 	{
					// 		tar_pos_ = opp_sort[1] + 100.0 / ((opp_sort[1] - ball_pos_).length()) * (opp_sort[1] - ball_pos_);
					// 	}
					// 	else
					// 	{
					// 		tar_pos_ = opp_sort[1] + 75.0 / ((ball_pos_ - opp_sort[1]).length()) * (ball_pos_ - opp_sort[1]);
					// 	}
					// }
					else //??????????????????????????? ?????????????????????
					{
					// 	DPoint dribble_pos = world_model_->RobotInfo_[our_dribble - 1].getLocation();
                    //     int tmpx = 10000, tmpy = 10000;
                    //     double max_dis = 0;
                    //     DPoint tmp;
                    //     for (int i = -500; i <= 0; i += 100) {
                    //         for (int j = -700; j <= 700; j += 100) {
                    //             tmp.x_ = double(i);
                    //             tmp.y_ = double(j);
                    //             if(tmp.distance(dribble_pos) > 1000) continue;
                    //             double dis = 0.0;
                    //             for(int opp = 0; opp < 5; opp++){
                    //                 DPoint oppp = world_model_->Opponents_[opp];
                    //                 dis += (((oppp - tmp).length()) * ((oppp - tmp).length()));
                    //             }
                    //             if (dis > max_dis) {
                    //                 max_dis = dis;
                    //                 tmpx = i;
                    //                 tmpy = j;
                    //             }
                    //         }
                    //     }
                    //     DPoint square_center = DPoint(double(tmpx), double(tmpy));
                    //     DPoint tmp2;
                    //     for (int i = -2; i <= 2; i++) {
                    //         for (int j = -2; j <= 2; j++) {
                    //             tmp2.x_ = square_center.x_ + i * 50;
                    //             tmp2.y_ - square_center.y_ + j * 50;
                    //             if(tmp2.distance(dribble_pos) > 1000) continue;
                    //             double dis = 0.0;
                    //             for(int opp = 0; opp < 5; opp++){
                    //                 DPoint oppp = world_model_->Opponents_[opp];
                    //                 dis += (((oppp - tmp).length()) * ((oppp - tmp).length()));
                    //             }
                    //             if (dis > max_dis) {
                    //                 tar_pos_ = tmp2;
                    //                 max_dis = dis;
                    //             }
                    //         }
                    //     }
					// }
					tar_pos_ = world_model_->middle_pt_;
					action->move_action = CatchBall;
					action->rotate_acton = CatchBall;
                	action->rotate_mode = 0;
					m_behaviour_.move2targetFast(tar_pos_,robot_pos_);
					}
				}
			}
			else {				
				DPoint tar_pos = DPoint(0.0,0.0);
				int mid_id = -1; //????????????
				double minlength = MAX;
				for (int i = 2; i <= 5; i++)
				{
					if (!ifParking(world_model_->RobotInfo_[i - 1].getLocation()) && i != our_near_ball_sort_ID[0] && i != our_nearest_oppgoal_ID) // except our robot which is the nearest to ball
					{
						if ((world_model_->RobotInfo_[i - 1].getLocation() - DPoint(0.0,0.0)).length() < minlength)
						{
							mid_id = i;
							minlength = (world_model_->RobotInfo_[i - 1].getLocation() - DPoint(0.0,0.0)).length();
						}
					}
				}
				
				if (world_model_->AgentID_ == our_nearest_oppgoal_ID) {
					pre_attack_2();
				}
				else if (world_model_->AgentID_ == mid_id){
					tar_pos = world_model_->middle_pt_;
				}
				else {
					tar_pos = world_model_->passive_pt_;
				}
				action->move_action = CatchBall;
				action->rotate_acton = CatchBall;
                action->rotate_mode = 0;
				m_behaviour_.move2targetFast(tar_pos,robot_pos_);
			}			
			return;
		}
}