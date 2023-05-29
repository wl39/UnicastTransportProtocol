# UnicastTransportProtocol
- 사용 언어: C
- UDP를 이용한 파일 전송 프로그램입니다.
	- 해당 프로그램은 slurpe-3라는 프로그램과 같이 실행되어야하는 프로그램입니다.
	- slurpe-3 프로그램은 Unix에서만 사용가능한 프로그램입니다.
		- Unix user ID와 포트넘버를 이용해 packet을 송수신 합니다.
		- packet 송수신에 해당하는 delay/loss/rate를 지정할 수 있습니다.
	- 이 프로그램은 인터넷 환경설정이 좋지 않은 경우에, Idle-RQ을 이용해 특정 data packet을 재송신 할 수 있습니다
- 해당 프로그램은 실행 불가능합니다.
- 개발기간: 14일
- 마지막 수정일: 2022년 4월

---
## 프로그램에서 주목해서 봐야할 점
- C를 이용한 UDP 통신
- Socket에 대한 이해
- RFC793(S)에 대한 이해
- FSM(Finite State Machine) 및 튜링 머신에 대한 이해
- Protocol의 Header에 대한 이해
- Idle-RQ에 대한 이해
