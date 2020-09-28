#include "defines.h"

// ����������� ticket lock
typedef struct
{
  // �����, ������� ����� ����� ��� ��������� ������� ������� ����������
  unsigned int Ticket;

  // ����� ������ ��������, �������� ��������� ��������� ����������
  unsigned int CurrentRead;

  // ����� ������ ��������, �������� ��������� ��������� ����������
  unsigned int CurrentWrite;
} RW_LOCKER;

// ������������� ����������
void Setup( RW_LOCKER *Locker )
{
  Locker->Ticket = 0;
  Locker->CurrentRead = 0;
  Locker->CurrentWrite = 0;
}

int GetTicket( RW_LOCKER *Locker )
{
  // ��������� ����������� ������
  return atomic_add(&Locker->Ticket, 1);
}

/* ������ atomic_cmpxchg( ... , (1 << 30), (1 << 30)) ����� ���� ������������ atomic_add( ... , 0).
 * (������������� atomic_load �������� �� ���������)
 */

void LockReader( RW_LOCKER *Locker )
{
  unsigned int CurrentTicket = GetTicket(Locker);

  // ��� ��� ��� ������ � 1 warp ������ ������������, ��������, ������ � ���
  while (CurrentTicket != atomic_cmpxchg(&Locker->CurrentRead, CurrentTicket, CurrentTicket + 1)) // ��� �������� ���������� � ������ ������
  {
  }
}

void UnlockReader( RW_LOCKER *Locker )
{
  // ����������� CurrentWrite <- ��������� �������� (���� ��� ��������) ������ ���� ����� �����
  atomic_add(&Locker->CurrentWrite, 1);
}

int LockWriter( RW_LOCKER *Locker, unsigned int CurrentTicket )
{
  // ������ ������ �� �������
  return CurrentTicket != atomic_cmpxchg(&Locker->CurrentWrite, (1 << 30), (1 << 30));
}

void UnlockWriter( __local RW_LOCKER *Locker )
{
  // ���������� ��������� �����
  atomic_add(&Locker->CurrentWrite, 1);
  atomic_add(&Locker->CurrentRead, 1);
}

__kernel do_some_work()
{
    assert(get_group_id == [256, 1, 1]);

    volatile __local RW_LOCKER Locker;

    if (get_local_id(0) == 0 && get_local_id(1) == 0 && get_local_id(2) == 0)
      Setup(&Locker); // �������� Setup 1 ���

    barrier(CLK_LOCAL_MEM_FENCE); // �������� ����, ��� ����� ���� ������ Locker ������������������

    __local disjoint_set = ...;

    for (int iters = 0; iters < 100; ++iters) {      // ������ ������ ��� ��������
        if (some_random_predicat(get_local_id(0))) { // �������� ����������� ����� ����� (�������� ���� - 0.1%)
          ...                        
            int CurrentTicket = GetTicket(&Locker);
            int LockedFlag;
            do
            {
              LockedFlag = LockWriter(&Locker, CurrentTicket);

              if (LockedFlag)
              {
                union(disjoint_set, ...);
                UnlockWriter(&Locker);
              }
              
            } while (!LockedFlag);  // ���� LockedFlag �� ���� ����� ���������� ��������� � �����(������ �� �����), ���� ��������� �� ������
          ...
        }
        ...
        LockReader(&Locker);
        tmp = get(disjoint_set, ...); // ������ ��������� ����� ������ �� ����������
        UnlockReader(&Locker);
        ...
    }
}
