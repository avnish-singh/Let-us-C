
#include<stdio.h>

int main(){
    int i=6;
    for(;i<=10;)
    {
        printf("%d\n",i);
        i++;
    }

/*    for(i=1;i<=10;)
    {
        printf("%d\n",i);
        i = i + 1;
    }
/*    for(i=1;i<=10;i=i+1)
        printf("%d\n", i);

/*     for(scanf("%d",&i);i<=10;i++)
        printf("%d\n",i);

//    for(i=1;i<=10;printf("%d",i++));
/*    for(i<4;j=5;j=0)
        printf("%d", i);// infinite loop not decrementing i;
/*    for(i=10;i;i--)
        printf("%d\n", i); */
/*    int i;
    printf("%d\n", i);
    while (i == 0)
    {
        printf("i :%d\n",i);
        i += 1;
    }*/
/*
    while('a' < 'b')
        printf("Malayalam is palindrome\n");
        //indefinite loop
*/
    /*int x=4, y=3, z;
    z = x-- - y;
    printf(" x :%d\n y :%d\n z :%d\n", x, y, z);
    */


/*
    char g;
    int yos,qual,sal = 0;
    printf("Enter Gender, Year of Service, Qualification ( 0 = G, 1 = PG ):");
    scanf("%c %d %d", &g,&yos, &qual);

    if(g=='M' && yos>=10 && qual==1)
    {
        sal = 15000;
        printf("Male PG >=10Y of service Salary : %d", sal);
    }
    else if((g=='M' && yos>=10 && qual==0) || (g=='M' && yos<10 && qual==1))
    {
        sal = 10000;
        printf("Male PG & G less then 10Y of service Salary : %d", sal);
    }
    else if(g=='M' && yos<10 && qual==0)
        printf("Graduate Male Under 10Y of service Salary : %d", sal = 7000);
    else if(g=='F' && yos>=10 && qual==1)
        printf("Post Graduate Female above 10Y of service Salary : %d", sal = 12000);
    else if(g=='F' && yos>=10 && qual==0)
        printf("Graduate Female above 10Y of service Salary : %d", sal = 9000);
    else if(g=='F' && yos<10 && qual==1)
        printf("Post Graduate Female under 10Y of service Salary : %d", sal = 10000);
    else if(g=='F' && yos<10 && qual==0)
        printf("Graduate Female under 10Y of service Salary : %d", sal = 6000);


    if(g=='M' && yos>=10 && qual==1)
        sal = 15000;
    else if((g=='M' && yos>=10 && qual==0) || (g=='M' && yos<10 && qual==1))
        sal = 10000;
    else if(g=='M' && yos<10 && qual==0)
        sal = 7000;
    else if(g=='F' && yos>=10 && qual==1)
        sal = 12000;
    else if(g=='F' && yos>=10 && qual==0)
        sal = 9000;
    else if(g=='F' && yos<10 && qual==1)
        sal = 10000;
    else if(g=='F' && yos<10 && qual==0)
        sal = 6000;
    printf("Salary : %d", sal);
*/
    return 0;
}
