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

/* ������ atomic_cmpxchg( ... , (1 << 30), (1 << 30)) ����� ���� ������������ atomic_add( ... , 0).
 * (������������� atomic_load �������� �� ���������)
 */

void LockReader( RW_LOCKER *Locker )
{
  // ��������� ����������� ������
  unsigned int CurrentTicket = atomic_add(&Locker->Ticket, 1);

  // ���� ���� ��� �������� �����
  while (CurrentTicket != atomic_cmpxchg(&Locker->CurrentRead, (1 << 30), (1 << 30))) // CurrentRead ������� �� ����� ����� (1 << 30) (� ���� ���� �����, �� �� ��� ����� ����� ��������� (1 << 30)) <- ��� ������������ ��������� �������� CurrentRead
  {
  }

  // ����������� CurrentRead <- ��������� �������� (���� ��� ��������) ����� �����
  atomic_add(&Locker->CurrentRead, 1);
}

void UnlockReader( RW_LOCKER *Locker )
{
  // ����������� CurrentWrite <- ��������� �������� (���� ��� ��������) ������ ���� ����� �����
  atomic_add(&Locker->CurrentWrite, 1);
}

void LockWriter( RW_LOCKER *Locker )
{
  // ��������� ����������� ������
  unsigned int CurrentTicket = atomic_add(&Locker->Ticket, 1);

  // ���� ���� ��� �������� �����
  while (CurrentTicket != atomic_cmpxchg(&Locker->CurrentWrite, (1 << 30), (1 << 30))) // CurrentWrite ������� �� ����� ����� (1 << 30) (� ���� ���� �����, �� �� ��� ����� ����� ��������� (1 << 30)) <- ��� ������������ ��������� �������� CurrentWrite
  {
  }

  // ������ ������ �� �������
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
            ...                        // �� ������ �������� ��������� ������
            LockWriter(&Locker);
            union(disjoint_set, ...);  // ����� �������� �������� ���� ����������
            UnlockWriter(&Locker);
            ...
        }
        ...
        LockReader(&Locker);
        tmp = get(disjoint_set, ...); // ������ ��������� ����� ������ �� ����������
        UnlockReader(&Locker);
        ...
    }
}
