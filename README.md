# xv6_thread_implementation

xv6上でthreadを実装する．

## 基本設計
スレッドの特徴は，生成元のプロセスとアドレス空間を共有し，スタック領域だけ独自のものを持つことである．

本実装では，既存のfork関数を改良し，新たなプロセスのページテーブル割り当ての処理を改良することでスレッドを実装した．


## threadの実装〜Kernel Side〜

### proc.c

**・clone()**

まず，スレッドの生成を行うカーネル側の関数である `clone()`について説明する．基本構造は，fork関数と同じである．
異なる点はページテーブルの割り当てを行うための関数として，`uvmcopy`を使用するのではなく，自作の関数である`uvmcopy_onlystack`を用いている点である．
```c
int
clone()
{
  //省略(fork()と同一）
  if(uvmcopy_onlystack(p->pagetable, np->pagetable, p->trapframe->sp, p->sz) < 0){
    freeproc(np);
    release(&np->lock);
    return -1;
  }
  //省略（fork()と同一）
  return pid;
}
```



**・thread_exit()**

スレッド用のexit関数．通常の`exit()`を使用してしまうと，アドレス空間を共有している他のプロセスの領域も解放してしまう．


thread_exit()では，自作の`uvmcopy_excludestack()`を用いたあとに，通常の`exit()`を用いることで，他のスレッドと共有している部分のデータは残したままスレッドの処理を終了する．
```c
int thread_exit(void){
  struct proc *p = myproc();
  uvmcopy_excludestack(p->pagetable,p->trapframe->sp,p->sz);
  exit(0);
  return 0;
}
```



**・join()**

親スレッドが，子スレッドの処理が終了するまで待機するための関数．引数で子スレッドのpidを取る．
```c
int join(int pid){
  wait(pid);
  return 0;
}
```

### vm.c
---

**・uvmcopy_onlystack()**

スタック領域以外のページテーブルエントリは親プロセスと同じものを参照し，スタック領域のページテーブルエントリのみ，新たに確保した領域に元々のプロセスのスタック領域をコピーしたものを参照する．
```c
int uvmcopy_onlystack(pagetable_t old, pagetable_t new, uint64 stack, uint64 sz)
{
  pte_t *pte;
  uint64 pa, i;
  uint flags;
  char *mem;

  for (i = 0; i < sz; i += PGSIZE)
  {
    if ((pte = walk(old, i, 0)) == 0)
      panic("uvmcopy: pte should exist");
    if ((*pte & PTE_V) == 0)
      panic("uvmcopy: page not present");
    pa = PTE2PA(*pte);
    flags = PTE_FLAGS(*pte);

    //Stack領域だった場合は親と親と同じページテーブルを参照
    if(i <= stack && i + PGSIZE > stack )
    {
      if ((mem = kalloc()) == 0)
        goto err;
      memmove(mem, (char *)pa, PGSIZE);
      if (mappages(new, i, PGSIZE, (uint64)mem, flags) != 0)
      {
        kfree(mem);
        goto err;
      }
    }
    else
    {
       if (mappages(new, i, PGSIZE, (uint64)pa, flags) != 0)
      {
        goto err;
      }
    }
  }
  return 0;

err:
  uvmunmap(new, 0, i / PGSIZE, 1);
  return -1;
}
```


**・uvmcopy_excludestack()**

スタック領域以外のページテーブルエントリを新たな物理メモリ領域にマッピングし直す関数．

この処理によって，`exit()`によって領域を解放しても，親スレッドは影響を受けることなく処理を続けることが可能になる．
```c
int uvmcopy_excludestack(pagetable_t pagetable, uint64 stack, uint64 sz)
{
  pte_t *pte;
  uint64 pa, i;
  uint flags;
  char *mem;

  for (i = 0; i < sz; i += PGSIZE)
  {
    if(i <= stack && i + PGSIZE > stack )
      continue;
    if ((pte = walk(pagetable, i, 0)) == 0)
      panic("uvmcopy: pte should exist");
    if ((*pte & PTE_V) == 0)
      panic("uvmcopy: page not present");
    pa = PTE2PA(*pte);
    flags = PTE_FLAGS(*pte);
    if ((mem = kalloc()) == 0)
        goto err;
    memmove(mem, (char *)pa, PGSIZE);
    if (mappages(pagetable, i, PGSIZE, (uint64)mem, flags) != 0)
    {
      printf("uvmcopy\n");
      kfree(mem);
      goto err;
    }
  }
  return 0;

err:
  uvmunmap(pagetable, 0, i / PGSIZE, 1);
  return -1;
}
```

## threadの実装〜Kernel Side〜

### thread.c
ユーザ側からは，`create_thread()`関数を使って，スレッドの生成を依頼する．引数には，スレッドに実行してほしい関数のアドレスとその関数の引数を渡す．
```c
```

### systemcalls
システムコールは，`clone()`，`join()`，`t_exit()`の３つを実装し，それぞれ前述したカーネル空間の関数である，`clone()`．`join()`，`thread_exit()`を呼び出す．


## testプログラムの実行結果
以下の，test．ｃを実行し，スレッドが正しく実装されているかどうかを確認した．

プログラムでは，ヒープ領域に変数`num`を定義し，子スレッド内でその値に10を足した後その結果が親スレッド内でも反映されているかどうかを確認している．
```c
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

void child(void *arg){
	printf("child called\n");
	int *num = (int*)arg;
	*num += 10;
	return;
}

int main(int argc, char *argv[]){
	int *num = malloc(sizeof(int));
	int ret;
	*num = 100;
	printf("before num : %d\n",*num);
	ret = create_thread(child,(void *)num);
	join(ret);
	printf("after num : %d\n",*num);
	exit(0);
}
```

実行結果は以下のようになった．numの値が元々の100から10を加算した110に変更されているのが確認できた．この結果からも，親スレッドと子スレッドの間でヒープ領域が確保できているのが確認できる．
```
$ test
before num : 100
child called
after num : 110
```



